#include "sewing_context.h"
#include "sewing_tele.h"
#include "statments.h"
#include "ConfigManager.h"
#include "core/cfgindex.h"
#include "core/wifi_mqtt.h"
#include "core/aquasew_topics.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <stdarg.h>

#define PF_CONTEXT_FILE "/pf_context.json"
#define ID_LEN   40
#define NAME_LEN 48

// ── Context state — Task2-owned ────────────────────────────────────────────────
static struct {
    // machine_config block
    char business_id[ID_LEN];
    char plant_id[ID_LEN];
    char layout_id[ID_LEN];
    char machine_id[ID_LEN];
    // active_manifest block
    char     manifest_id[ID_LEN];
    char     session_id[ID_LEN];
    char     style_id[ID_LEN];
    char     style_name[NAME_LEN];
    uint32_t manifest_version;   // in-block application version (drives R2)
    int      target_per_hour;
    bool     manifest_open;      // a manifest is loaded and not closed
} s_ctx;

static bool s_counting = true;   // count_total increments unless a manifest is closed

// ── Block delivery queue: Task1 (apply handler) -> Task2 (tick) ────────────────
struct ContextBlock {
    char key[24];
    char json[1024];   // raw attr/res payload (envelope + block); truncates safely
};
static QueueHandle_t s_block_q = nullptr;

// ── Persistence ────────────────────────────────────────────────────────────────
// Persist the manifest/machine identity so a reboot mid-job can tell R1 (new)
// from continue (same manifest_id) — this is doc/11C sec 9 reconnect logic.
static void context_save() {
    StaticJsonDocument<512> doc;
    doc["business_id"]      = s_ctx.business_id;
    doc["plant_id"]         = s_ctx.plant_id;
    doc["layout_id"]        = s_ctx.layout_id;
    doc["machine_id"]       = s_ctx.machine_id;
    doc["manifest_id"]      = s_ctx.manifest_id;
    doc["session_id"]       = s_ctx.session_id;
    doc["style_id"]         = s_ctx.style_id;
    doc["style_name"]       = s_ctx.style_name;
    doc["manifest_version"] = s_ctx.manifest_version;
    doc["target_per_hour"]  = s_ctx.target_per_hour;
    doc["manifest_open"]    = s_ctx.manifest_open;

    File f = LittleFS.open(PF_CONTEXT_FILE, "w");
    if (!f) { Serial.println(F("context: cannot write pf_context.json")); return; }
    serializeJson(doc, f);
    f.close();
}

static void context_load() {
    memset(&s_ctx, 0, sizeof(s_ctx));
    File f = LittleFS.open(PF_CONTEXT_FILE, "r");
    if (!f) { Serial.println(F("context: no pf_context.json — starting unassigned")); return; }
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, f)) { f.close(); Serial.println(F("context: pf_context.json parse error")); return; }
    f.close();

    strlcpy(s_ctx.business_id, doc["business_id"] | "", ID_LEN);
    strlcpy(s_ctx.plant_id,    doc["plant_id"]    | "", ID_LEN);
    strlcpy(s_ctx.layout_id,   doc["layout_id"]   | "", ID_LEN);
    strlcpy(s_ctx.machine_id,  doc["machine_id"]  | "", ID_LEN);
    strlcpy(s_ctx.manifest_id, doc["manifest_id"] | "", ID_LEN);
    strlcpy(s_ctx.session_id,  doc["session_id"]  | "", ID_LEN);
    strlcpy(s_ctx.style_id,    doc["style_id"]    | "", ID_LEN);
    strlcpy(s_ctx.style_name,  doc["style_name"]  | "", NAME_LEN);
    s_ctx.manifest_version = doc["manifest_version"] | 0;
    s_ctx.target_per_hour  = doc["target_per_hour"]  | 0;
    s_ctx.manifest_open    = doc["manifest_open"]    | false;
    // Counting resumes on boot UNLESS we booted into a closed manifest (a stored
    // manifest_id whose manifest_open is false). No manifest at all = standalone
    // = counting on.
    s_counting = !(s_ctx.manifest_id[0] && !s_ctx.manifest_open);
    Serial.printf("context: loaded manifest_id='%s' open=%d\n",
        s_ctx.manifest_id, s_ctx.manifest_open ? 1 : 0);
}

// ── cfgIndex apply handler — runs on Task1, MUST be quick ──────────────────────
// Copies the raw block into the queue; parsing + applying happens on Task2.
static void on_block_delivered(const char* key, const char* json) {
    if (!s_block_q) return;
    ContextBlock b = {};
    strlcpy(b.key,  key,  sizeof(b.key));
    strlcpy(b.json, json, sizeof(b.json));
    if (xQueueSendToBack(s_block_q, &b, 0) != pdPASS)
        Serial.printf("context: block queue full — %s dropped\n", key);
}

// ── Envelope + events ──────────────────────────────────────────────────────────

// Bounds-safe append: writes at buf[off..], never past len-1, returns the new
// offset (clamped to len-1 on truncation so a caller's buf+off can never run
// off the end — the size_t underflow of len-off is the bug this prevents).
static int bappend(char* buf, int off, size_t len, const char* fmt, ...) {
    if (off < 0 || (size_t)off >= len) return (int)len - 1;
    va_list ap; va_start(ap, fmt);
    int w = vsnprintf(buf + off, len - off, fmt, ap);
    va_end(ap);
    if (w < 0) return off;
    off += w;
    return ((size_t)off >= len) ? (int)len - 1 : off;
}

// One id field: "key":"value"  or  "key":null when the value is empty.
static int append_id(char* buf, int off, size_t len, const char* key, const char* val) {
    return (val && val[0]) ? bappend(buf, off, len, "\"%s\":\"%s\"", key, val)
                           : bappend(buf, off, len, "\"%s\":null", key);
}

int sewing_context_append_ids(char* buf, size_t len) {
    int n = 0;
    n = append_id(buf, n, len, "business_id", s_ctx.business_id);  n = bappend(buf, n, len, ",");
    n = append_id(buf, n, len, "plant_id",    s_ctx.plant_id);     n = bappend(buf, n, len, ",");
    n = append_id(buf, n, len, "layout_id",   s_ctx.layout_id);    n = bappend(buf, n, len, ",");
    n = append_id(buf, n, len, "machine_id",  s_ctx.machine_id);   n = bappend(buf, n, len, ",");
    n = append_id(buf, n, len, "manifest_id", s_ctx.manifest_open ? s_ctx.manifest_id : ""); n = bappend(buf, n, len, ",");
    n = append_id(buf, n, len, "session_id",  s_ctx.manifest_open ? s_ctx.session_id  : ""); n = bappend(buf, n, len, ",");
    n = append_id(buf, n, len, "operator_id", "");   // reserved, always null in AquaSew
    return n;
}

// epoch ms if NTP has synced (year > 2020), else 0 = "unknown, bridge stamps it".
static uint64_t epoch_ms_or_zero() {
    time_t now = time(nullptr);
    if (now < 1600000000) return 0;   // ~2020-09; before this the clock is unset
    return (uint64_t)now * 1000ULL;
}

void sewing_event_emit(const char* event_type, const char* payload_json) {
    char buf[MQTT_TX_PAYLOAD_MAX];
    int n = bappend(buf, 0, sizeof(buf), "{\"event_type\":\"%s\",", event_type);
    uint64_t ts = epoch_ms_or_zero();
    if (ts) n = bappend(buf, n, sizeof(buf), "\"ts\":%llu,", (unsigned long long)ts);
    n += sewing_context_append_ids(buf + n, sizeof(buf) - n);
    n = bappend(buf, n, sizeof(buf), ",\"payload\":%s}",
                (payload_json && payload_json[0]) ? payload_json : "{}");
    mqtt_tx_enqueue(AQ_TOPIC_TELE, buf, false);
    Serial.printf("event: %s\n", event_type);
}

static uint32_t s_session_seq = 0;
uint32_t sewing_context_session_seq() { return s_session_seq; }

bool sewing_context_counting_enabled() { return s_counting; }

// ── Block application (Task2) ──────────────────────────────────────────────────

// Unwrap the TB gateway envelope {"id","device","value":{...}} down to the block.
static JsonObjectConst unwrap(JsonDocument& doc) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    if (root.containsKey("data"))  return root["data"];
    if (root.containsKey("value")) return root["value"];
    return root;
}

static void apply_machine_config(const char* json) {
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, json)) { Serial.println(F("context: machine_config parse error")); return; }
    JsonObjectConst b = unwrap(doc);
    // Unrecognised fields are ignored and preserved-by-omission (forward compat).
    strlcpy(s_ctx.business_id, b["business_id"] | s_ctx.business_id, ID_LEN);
    strlcpy(s_ctx.plant_id,    b["plant_id"]    | s_ctx.plant_id,    ID_LEN);
    strlcpy(s_ctx.layout_id,   b["layout_id"]   | s_ctx.layout_id,   ID_LEN);
    strlcpy(s_ctx.machine_id,  b["machine_id"]  | s_ctx.machine_id,  ID_LEN);

    // input_mode is a provisioning fact ("this machine is a PLC/Modbus type").
    // ABSENT means LEAVE UNCHANGED (backend interop condition) — the manifest-push
    // widget writes machine_config without this field on most devices, and an
    // absent-means-0 would silently revert a SUPREM to GPIO. Only an explicit
    // value changes it; when it does, persist so it survives reboot.
    if (b.containsKey("input_mode")) {
        uint8_t m = (b["input_mode"].as<int>() != 0) ? 1 : 0;
        if (m != structSysConfig.input_mode) {
            structSysConfig.input_mode = m;
            ConfigManager::saveSystemConfig(structSysConfig);
            Serial.printf("context: machine_config input_mode -> %u (persisted)\n", m);
        }
    }

    Serial.printf("context: machine_config applied — machine_id='%s' plant='%s'\n",
        s_ctx.machine_id, s_ctx.plant_id);
    context_save();
}

static void apply_active_manifest(const char* json) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, json)) { Serial.println(F("context: active_manifest parse error")); return; }
    JsonObjectConst b = unwrap(doc);

    const char* new_id  = b["manifest_id"] | "";
    const char* new_sid = b["session_id"]  | "";
    const char* status  = b["status"]      | "open";
    uint32_t    ver     = b["version"]     | 0;
    bool        closed  = (strcmp(status, "closed") == 0);

    // R4 — no manifest / id cleared → idle, standalone counting resumes.
    if (!new_id[0]) {
        if (s_ctx.manifest_id[0]) Serial.println(F("context: manifest cleared -> idle (R4)"));
        s_ctx.manifest_id[0] = 0; s_ctx.session_id[0] = 0;
        s_ctx.manifest_open = false;
        s_counting = true;
        context_save();
        return;
    }

    // R1 — new manifest_id → new job.
    if (strcmp(new_id, s_ctx.manifest_id) != 0) {
        strlcpy(s_ctx.manifest_id, new_id,  ID_LEN);
        strlcpy(s_ctx.session_id,  new_sid, ID_LEN);
        strlcpy(s_ctx.style_id,   b["style_id"]   | "", ID_LEN);
        strlcpy(s_ctx.style_name, b["style_name"] | "", NAME_LEN);
        s_ctx.target_per_hour  = b["target_per_hour"] | 0;
        s_ctx.manifest_version = ver;
        s_ctx.manifest_open    = !closed;
        s_counting             = !closed;
        // Reset the session counter — same task (Task2) as the increment, so this
        // is the single-writer reset, not a cross-task race.
        structSysData.count_total = 0;
        s_session_seq++;                 // signal Modbus path to rebaseline
        ConfigManager::saveSystemData(structSysData);
        context_save();
        Serial.printf("context: NEW manifest '%s' session '%s' (R1) — count_total=0\n",
            s_ctx.manifest_id, s_ctx.session_id);
        sewing_event_emit("manifest_loaded", nullptr);
        sewing_tele_request_publish();   // show the fresh (zeroed) session at once
        return;
    }

    // Same manifest_id from here on.
    // R3 — closed.
    if (closed && s_ctx.manifest_open) {
        s_ctx.manifest_open = false;
        s_counting = false;
        context_save();
        Serial.printf("context: manifest '%s' CLOSED (R3) — counting stopped\n", s_ctx.manifest_id);
        char pl[48];
        snprintf(pl, sizeof(pl), "{\"count_total\":%u}", structSysData.count_total);
        sewing_event_emit("manifest_closed", pl);
        return;
    }

    // R2 — same id, higher version → update optional fields only, NO reset.
    if (ver > s_ctx.manifest_version) {
        strlcpy(s_ctx.style_id,   b["style_id"]   | s_ctx.style_id,   ID_LEN);
        strlcpy(s_ctx.style_name, b["style_name"] | s_ctx.style_name, NAME_LEN);
        s_ctx.target_per_hour  = b["target_per_hour"] | s_ctx.target_per_hour;
        s_ctx.manifest_version = ver;
        if (!closed) { s_ctx.manifest_open = true; s_counting = true; }
        context_save();
        Serial.printf("context: manifest '%s' updated to v%u (R2)\n", s_ctx.manifest_id, ver);
    }
    // else: same id + same-or-lower version → re-fetch of what we have, ignore.
}

// ── Public API ─────────────────────────────────────────────────────────────────
void sewing_context_init() {
    s_block_q = xQueueCreate(2, sizeof(ContextBlock));
    if (!s_block_q) Serial.println(F("context: block queue creation failed"));
    context_load();
    cfgindex_set_apply_handler(on_block_delivered);
}

void sewing_context_tick() {
    if (!s_block_q) return;
    ContextBlock b;
    while (xQueueReceive(s_block_q, &b, 0) == pdPASS) {
        if      (strcmp(b.key, "machine_config")  == 0) apply_machine_config(b.json);
        else if (strcmp(b.key, "active_manifest") == 0) apply_active_manifest(b.json);
        else Serial.printf("context: unknown block key '%s'\n", b.key);
    }
}
