#include "sewing_tele.h"
#include "sewing_context.h"
#include "statments.h"
#include "thermo.h"
#include "core/wifi_mqtt.h"
#include "core/aquasew_topics.h"
#include "default_config.h"
#include <WiFi.h>
#include <stdarg.h>

// Publish pacing. structSysConfig.updates_interval is operator-configurable via
// the web UI; an unset or absurd value falls back rather than spamming the broker.
#define TELE_INTERVAL_FALLBACK_S  DEFAULT_TELE_INTERVAL_S
#define TELE_INTERVAL_MAX_S       86400
#define HEARTBEAT_INTERVAL_S      DEFAULT_HEARTBEAT_INTERVAL_S

static uint32_t s_lastPublishMs   = 0;
static bool     s_forcePublish    = false;
static uint32_t s_lastHeartbeatMs = 0;
static bool     s_bootEmitted     = false;
static bool     s_wasOnline       = false;

// Machine state in PrimeFlow's runtime_state vocabulary (doc/11C). The internal
// eMC_state maps busy->running; idle and fault pass through. support/downtime are
// added later by the mechanic layer — additive, no remap here.
static const char* runtime_state_str() {
    switch (curruntMCstate) {
        case MC_BUSY:  return "running";
        case IDLE:     return "idle";
        case MC_FAULT: return "fault";
        default:       return "idle";
    }
}

// Bounds-safe append — never runs off the buffer even if the frame would exceed
// it (prevents the size_t underflow of len-off). Same idiom as sewing_context.
static int bappend(char* buf, int off, size_t len, const char* fmt, ...) {
    if (off < 0 || (size_t)off >= len) return (int)len - 1;
    va_list ap; va_start(ap, fmt);
    int w = vsnprintf(buf + off, len - off, fmt, ap);
    va_end(ap);
    if (w < 0) return off;
    off += w;
    return ((size_t)off >= len) ? (int)len - 1 : off;
}

// Build the frame and hand it to the TX queue. Task2 context only — every field
// below is owned by this task, so the snapshot needs no lock and cannot be torn
// by a concurrent update.
static void publish_tele() {
    unsigned int production = structSysData.productionCounter;  // lifetime odometer
    unsigned int count_tot  = structSysData.count_total;        // this-manifest session
    unsigned int powerT     = structSysData.powerTime;
    unsigned int runT       = structSysData.runTime;

    int    wrssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    String ip    = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "0.0.0.0";
    unsigned long upTime = millis() / 1000;

    char buf[MQTT_TX_PAYLOAD_MAX];
    // 7-id PrimeFlow context envelope first, so every frame is self-attributing.
    int n = bappend(buf, 0, sizeof(buf), "{");
    n += sewing_context_append_ids(buf + n, sizeof(buf) - n);
    n = bappend(buf, n, sizeof(buf),
        ",\"msgTyp\":\"update\",\"id\":\"%s\",\"mac\":\"%s\",\"fw_ver\":\"" FW_VER "\""
        ",\"runtime_state\":\"%s\""
        ",\"count_total\":%u,\"ProductionCount\":%u,\"PowerOn\":%u,\"runTime\":%u"
        ",\"ip\":\"%s\",\"rssi\":%d,\"upTime\":%lu",
        AQ_DEVICE_MAC, macStr, runtime_state_str(),
        count_tot, production, powerT, runT,
        ip.c_str(), wrssi, upTime);

#ifdef THERMO_OK
    n = bappend(buf, n, sizeof(buf), ",\"temp\":%.1f", get_temperatureC());
#endif
    bappend(buf, n, sizeof(buf), "}");

    mqtt_tx_enqueue(AQ_TOPIC_TELE, buf, false);
    s_lastPublishMs = millis();
    s_forcePublish  = false;
}

void sewing_tele_request_publish() {
    s_forcePublish = true;
}

void sewing_tele_init() {
    s_lastPublishMs   = millis();
    s_lastHeartbeatMs = millis();
    s_forcePublish    = false;
    prevMCstate       = curruntMCstate;   // no spurious transition publish at boot
}

void sewing_tele_tick() {
    // boot event — once, on the first tick that finds the link up (so the
    // envelope + any restored manifest context are already loaded).
    if (!s_bootEmitted && g_mqtt_online) {
        sewing_event_emit("boot", nullptr);
        s_bootEmitted = true;
        s_wasOnline   = true;
    }
    // online event — on the reconnect edge (down -> up), after the first boot.
    if (s_bootEmitted && g_mqtt_online && !s_wasOnline) {
        sewing_event_emit("online", nullptr);
    }
    s_wasOnline = g_mqtt_online;

    // Machine-state transitions are rare and worth reporting immediately — this
    // is the only event-driven telemetry publish. Production pulses deliberately
    // do NOT publish: the counters are monotonic, so the next periodic snapshot
    // already carries every increment since the last one.
    if (curruntMCstate != prevMCstate) {
        prevMCstate    = curruntMCstate;
        s_forcePublish = true;
    }

    uint32_t interval_s = structSysConfig.updates_interval;
    if (interval_s == 0 || interval_s > TELE_INTERVAL_MAX_S)
        interval_s = TELE_INTERVAL_FALLBACK_S;

    if (s_forcePublish || (uint32_t)(millis() - s_lastPublishMs) >= interval_s * 1000UL)
        publish_tele();

    // Low-rate heartbeat event so the backend has an explicit liveness signal
    // distinct from the counter telemetry.
    if (g_mqtt_online && (uint32_t)(millis() - s_lastHeartbeatMs) >= HEARTBEAT_INTERVAL_S * 1000UL) {
        s_lastHeartbeatMs = millis();
        sewing_event_emit("heartbeat", nullptr);
    }
}
