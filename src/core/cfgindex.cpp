#include "cfgindex.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/semphr.h>

// ── Internal state ─────────────────────────────────────────────────────────────

// Per-slot state. local_ver is persisted to /cfgindex.bin; server_ver and
// pending are in-memory only.
static struct {
    uint64_t local_ver;  // version we last saved for this slot (from disk)
    uint64_t server_ver; // version the server reported in cfgIndex (current session)
    bool     pending;    // we requested this slot, awaiting delivery on attr/set
} s_block[CFGINDEX_SLOTS];

// Protects s_block[] and all LittleFS operations.
static SemaphoreHandle_t s_mutex = nullptr;

// ── Slot key ↔ index mapping ───────────────────────────────────────────────────
// Names match PrimeFlow's shared-attribute keys (doc/11C sec 6.1) so nothing
// gets renamed on the way to PrimeFlow. Add a slot here and register a consumer
// via cfgindex_set_apply_handler to make a new block server-pushable.
static const char* const kSlotKeys[CFGINDEX_SLOTS] = {
    "machine_config",   // CFGSLOT_MACHINE_CONFIG  — device<->machine binding
    "active_manifest",  // CFGSLOT_ACTIVE_MANIFEST — the active production job
};

// Product-supplied consumer for fetched block content (see cfgindex.h).
static cfgindex_apply_fn s_apply_handler = nullptr;

void cfgindex_set_apply_handler(cfgindex_apply_fn fn) { s_apply_handler = fn; }

static int8_t key_to_idx(const char* key) {
    for (uint8_t i = 0; i < CFGINDEX_SLOTS; i++)
        if (strcmp(key, kSlotKeys[i]) == 0) return (int8_t)i;
    return -1;
}

static void idx_to_key(uint8_t idx, char* out, size_t len) {
    if (idx >= CFGINDEX_SLOTS) { if (len) out[0] = '\0'; return; }
    strncpy(out, kSlotKeys[idx], len);
    out[len - 1] = '\0';
}

// ── CfgIndex file I/O ──────────────────────────────────────────────────────────

// Load persisted local versions from /cfgindex.bin into s_block[].
// Called once in config_init() — caller must NOT hold the mutex yet.
static void load_cfgindex_from_file() {
    File f = LittleFS.open(CFGINDEX_FILE, "r");
    if (!f) return; // no file yet — all local_ver stay 0 (never synced)

    uint32_t magic = 0;
    f.read((uint8_t*)&magic, sizeof(magic));
    // 0x4346475B ("CFG[") — bumped when the slots were renamed to
    // machine_config / active_manifest. An older file is discarded and re-synced.
    if (magic != 0x4346475B) {
        Serial.println("cfgindex: bad/old magic — treating as empty");
        f.close();
        return;
    }

    // Read one uint64_t per version-store slot (schedule blocks + config tables)
    for (int i = 0; i < CFGINDEX_SLOTS; i++) {
        f.read((uint8_t*)&s_block[i].local_ver, sizeof(uint64_t));
    }
    f.close();
    Serial.println("cfgindex: local versions loaded from /cfgindex.bin");
}

// Persist the current local_ver values to /cfgindex.bin.
// Caller must already hold s_mutex.
static void save_cfgindex_to_file() {
    File f = LittleFS.open(CFGINDEX_FILE, "w");
    if (!f) {
        Serial.println("cfgindex: cannot write /cfgindex.bin");
        return;
    }
    uint32_t magic = 0x4346475B;
    f.write((uint8_t*)&magic, sizeof(magic));
    for (int i = 0; i < CFGINDEX_SLOTS; i++) {
        f.write((uint8_t*)&s_block[i].local_ver, sizeof(uint64_t));
    }
    f.close();
}

// ── Public API ─────────────────────────────────────────────────────────────────

void config_init() {
    // true = format the partition if mounting fails (handles first boot / corruption)
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed — filesystem unavailable");
        return;
    }
    Serial.println("LittleFS mounted OK");

    // Restore the local cfgIndex versions from disk before creating the mutex
    // so tasks cannot race with this initialisation sequence.
    load_cfgindex_from_file();

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) Serial.println("config_init: mutex creation failed");
}

// Process incoming cfgIndex push from ThingsBoard.
// Marks blocks whose server version is newer than our local version as pending,
// and returns their key strings so the caller can request them via attr/req.
int cfgindex_process(const char* json, char stale_keys[][16], uint8_t max_keys) {
    if (!s_mutex) return 0;

    // 8 KB covers the full per-block cfgIndex version map at up to 64 blocks.
    // `static` (BSS, not stack) — stack-allocating 8 KB here while Task1 also
    // has 6 KB of cfgIndex publish buffers in the same call chain blows the
    // 20 KB task stack. s_mutex (held by the caller) prevents concurrent access,
    // so single-instance is safe.
    static StaticJsonDocument<8192> doc;
    doc.clear();  // wipe previous content — required because the doc is reused
    if (deserializeJson(doc, json)) {
        Serial.println("cfgindex_process: JSON parse error");
        return 0;
    }

    // Unwrap the outer envelope — gateway push uses "data", direct attr/res uses "value"
    JsonObject root = doc.containsKey("data")  ? doc["data"].as<JsonObject>()  :
                      doc.containsKey("value") ? doc["value"].as<JsonObject>() :
                      doc.as<JsonObject>();

    // attr/set push wraps cfgIndex under a "cfgIndex" key inside the envelope.
    // attr/res to a direct "cfgIndex" request returns the cfgIndex content at root level
    // (no extra nesting) — detect by checking if a "cfgIndex" key exists.
    JsonObject ci = root.containsKey("cfgIndex") ? root["cfgIndex"].as<JsonObject>() : root;
    if (ci.isNull()) {
        Serial.println("cfgindex_process: could not locate cfgIndex content");
        return 0;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) return 0;

    int count = 0;
    for (JsonPair kv : ci) {
        const char* key = kv.key().c_str();
        int8_t idx = key_to_idx(key);
        if (idx < 0) continue; // skip unknown keys

        uint64_t server_ver = kv.value()["ver"].as<uint64_t>();
        s_block[idx].server_ver = server_ver;

        if (server_ver > s_block[idx].local_ver) {
            // This block is newer on the server — mark it pending and queue for fetch
            s_block[idx].pending = true;
            if (count < max_keys) {
                strncpy(stale_keys[count], key, 15);
                stale_keys[count][15] = '\0';
                count++;
            }
            Serial.printf("cfgIndex: %s stale (local=%llu server=%llu)\n",
                key,
                (unsigned long long)s_block[idx].local_ver,
                (unsigned long long)server_ver);
        }
    }

    xSemaphoreGive(s_mutex);
    return count;
}

// Check whether the given block key was marked pending by cfgindex_process().
bool cfgindex_is_pending(const char* block_key) {
    int8_t idx = key_to_idx(block_key);
    if (idx < 0) return false;
    // No mutex needed — reading a bool is atomic on 32-bit ARM
    return s_block[idx].pending;
}

// Return the key of the first pending block so the fetch state machine knows
// what to request next.
bool cfgindex_next_pending(char* key_out) {
    for (int i = 0; i < CFGINDEX_SLOTS; i++) {
        if (s_block[i].pending) {
            idx_to_key((uint8_t)i, key_out, 16);
            return true;
        }
    }
    return false;
}

// Clear the pending flag for a block without updating local_ver — used when
// the fetch state machine gives up after exhausting retries for that block.
void cfgindex_clear_pending(const char* block_key) {
    int8_t idx = key_to_idx(block_key);
    if (idx >= 0) s_block[idx].pending = false;
}

// Build the local cfgIndex JSON for publishing to attr/pub.
// Only includes blocks that have been successfully synced (local_ver > 0).
void cfgindex_get_local_json(char* buf, size_t len) {
    // 3 KB covers the full 64-block map at the longest "ver":<uint32> form
    // (~32 bytes/block × 64 = ~2 KB content + ArduinoJson per-key overhead).
    // Matches MQTT_MAX_PACKET_SIZE so anything that fits this doc also fits
    // the outbound publish path.
    StaticJsonDocument<3072> doc;
    JsonObject root = doc.to<JsonObject>();
    for (int i = 0; i < CFGINDEX_SLOTS; i++) {
        if (s_block[i].local_ver > 0) {
            char key[16];
            idx_to_key((uint8_t)i, key, sizeof(key));
            root[key]["ver"] = s_block[i].local_ver;
        }
    }
    serializeJson(doc, buf, len);
}

// Zero local version(s) so the next sync re-fetches from the server.
// Version-only — leaves config data files untouched.
int cfgindex_reset(const char* key) {
    if (!s_mutex) return -1;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) return -1;

    int reset = 0;
    if (!key || !*key || strcmp(key, "all") == 0) {
        for (int i = 0; i < CFGINDEX_SLOTS; i++) {
            if (s_block[i].local_ver != 0) reset++;
            s_block[i].local_ver = 0;
            s_block[i].pending   = false;   // fetch SM re-evaluates from scratch
        }
        Serial.printf("cfgindex_reset: ALL slots zeroed (%d had a version)\n", reset);
    } else {
        int8_t idx = key_to_idx(key);
        if (idx < 0) {
            Serial.printf("cfgindex_reset: unknown key '%s'\n", key);
            xSemaphoreGive(s_mutex);
            return -1;
        }
        s_block[idx].local_ver = 0;
        s_block[idx].pending   = false;
        reset = 1;
        Serial.printf("cfgindex_reset: %s zeroed\n", key);
    }
    save_cfgindex_to_file();
    xSemaphoreGive(s_mutex);
    return reset;
}

int config_factory_reset() {
    // Every persisted config file. /wifi.json (credentials + portal flag) and
    // /counter.txt are deliberately NOT listed — a factory reset must keep the
    // device reachable and preserves the boot counter. No mutex: this is only
    // ever called from the factory_reset RPC, which reboots immediately after,
    // so nothing else races these files before the reset takes hold.
    static const char* const files[] = {
        CFGINDEX_FILE,
    };
    int removed = 0;
    for (uint8_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        if (LittleFS.exists(files[i]) && LittleFS.remove(files[i])) {
            removed++;
            Serial.printf("factory_reset: removed %s\n", files[i]);
        }
    }
    Serial.printf("factory_reset: %d file(s) removed (wifi.json preserved)\n", removed);
    return removed;
}

// Apply a fetched cfgIndex payload and advance its local version. The attr fetch
// state machine calls this for every delivered key.
bool cfgindex_save(const char* key, const char* json) {
    int8_t idx = key_to_idx(key);
    if (idx < 0) {
        Serial.printf("cfgindex_save: unknown key '%s'\n", key);
        return false;
    }

    // Hand the raw block to the product consumer (Task2 for this device — the
    // handler just enqueues; parsing + applying happens on the owning task). The
    // core stays domain-agnostic. If no consumer is registered the block is
    // simply versioned and dropped — the pre-consumer behaviour.
    if (s_apply_handler) {
        s_apply_handler(key, json);
    } else {
        Serial.printf("cfgindex_save: %s received (%u bytes) — no consumer registered\n",
            key, (unsigned)strlen(json));
    }

    // Advance the TRANSPORT version so the block is fetched once, not every sync
    // cycle. This is intentionally independent of the block's own application
    // version (the "version" field inside an active_manifest, which the consumer
    // tracks separately and which resets per manifest). Never unify the two:
    // the transport version must stay monotonic or the stale-compare breaks.
    if (!s_mutex) return false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) return false;
    s_block[idx].local_ver = s_block[idx].server_ver;
    s_block[idx].pending   = false;
    save_cfgindex_to_file();
    xSemaphoreGive(s_mutex);

    Serial.printf("cfgindex_save: %s version advanced to %llu\n",
        key, (unsigned long long)s_block[idx].local_ver);
    return true;
}

