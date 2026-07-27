// WiFi transport MQTT module.
// Compiled only when -D WLAN is set. Provides the same public API as gprs_mqtt.cpp
// so main.cpp needs zero transport-specific code.
//
// Architecture is identical to gprs_mqtt.cpp (BlackWire mqtt_improve pattern):
//   callback()               — dumb pipe: enqueue to xQueue_mqtt_inbound only
//   com_loop()               — connect → client.loop() → drain inbound → drain TX → tick SM
//   drain_mqtt_inbound_queue — routes msgs to handle_rpc / handle_attr_res / handle_attr_set
//   drain_mqtt_tx_queue      — publishes TX queue via client.publish()
//   tick_attr_fetch_sm()     — 5-state cfgIndex sync state machine
//   mqtt_tx_enqueue()        — safe enqueue from any context
#ifdef WLAN

#include "wifi_mqtt.h"
#include "cfgindex.h"
#include "led_status.h"     // led_status_set_ota — status pixel during OTA
#include "OTA.h"
#include "wifi_creds.h"     // for the enter_portal RPC handler below
#include "domain_cmd.h"     // RPCs that mutate domain state post, never write
#include "../statments.h"   // structSysConfig — hw_caps identity on publish_info()
#include "../sewing_cmd.h"  // SEW_CMD_* ids for the domain RPC branches
#include "../default_config.h"

// ── TLS certificate ────────────────────────────────────────────────────────────
static const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=" \
"-----END CERTIFICATE-----\n";

// ── MQTT broker credentials ────────────────────────────────────────────────────
// NOTE: broker host/port are compile-time here (the core's design) rather than
// read from structSysConfig like the retired tbBroker was — the CA cert above
// pins this specific EMQX Cloud endpoint, so a runtime host change would need a
// matching cert anyway. Credentials are this project's EMQX user; the backend
// agent must confirm its ACL covers AQ_TOPIC_ROOT/<MAC>/# (see the change request).
static const char* mqttServer   = DEFAULT_MQTT_HOST;
static const int   mqttPort     = DEFAULT_MQTT_PORT;
static const char* mqttUser     = DEFAULT_MQTT_USER;
static const char* mqttPassword = DEFAULT_MQTT_PASS;

// ── Transport stack ────────────────────────────────────────────────────────────
static WiFiClientSecure wifiSecureClient;
PubSubClient client(wifiSecureClient);

// MQTT link state for the status LEDs (pixel [3]). The LED poll runs on loopTask
// and must NOT call client.connected() itself — PubSubClient/SSLClient are not
// thread-safe and touching them off Task1 corrupts the session. It reads this
// flag, which the MQTT task maintains, instead.
volatile bool g_mqtt_online = false;

// ── FreeRTOS queues ────────────────────────────────────────────────────────────
QueueHandle_t xQueue_mqtt_tx      = nullptr;
QueueHandle_t xQueue_mqtt_inbound = nullptr;

// ── WiFi readiness flag ────────────────────────────────────────────────────────
// Set by the WiFi GOT_IP event handler — tells com_loop() it is safe to connect.
// Doing MQTT connect directly inside a WiFi event callback is unsafe because
// it runs in the WiFi task context; we only set a flag there instead.
static volatile bool wifi_ready = false;

// ── Attr fetch state machine ───────────────────────────────────────────────────
enum eAttrFetchState {
    AFS_IDLE,
    AFS_FETCH_META,
    AFS_WAIT_META,
    AFS_FETCH_KEY,
    AFS_WAIT_KEY,
};

static eAttrFetchState g_attrFetchState      = AFS_IDLE;
static uint32_t        g_attrRetryAfterMs    = 1000;
static uint32_t        g_attrFetchLastMs     = 0;
static uint16_t        g_attrFetchReqId      = 0;
static uint16_t        g_attrFetchWaitId     = 0;
static char            g_attrFetchKey[16]    = {};
static uint8_t         g_attrFetchKeyRetries = 0;

#define kAttrFetchTimeoutMs        6000
#define kAttrFetchMaxRetries       3
#define kAttrFetchRetryIntervalMs  300000

// ── Internal helpers ───────────────────────────────────────────────────────────

// Direct publish — ONLY from MQTT task context (com_loop, drains, state machine).
static void publish_direct(const char* topic, const char* payload, bool retained = false) {
    bool ok = client.publish(topic, payload, retained);
    Serial.printf("MQTT tx [%s]: %s [%s]\n", topic, payload, ok ? "ok" : "FAIL");
}

// ── Public TX enqueue ──────────────────────────────────────────────────────────
bool mqtt_tx_enqueue(const char* topic, const char* payload, bool retained) {
    if (!xQueue_mqtt_tx) return false;
    MqttTxMsg msg = {};
    strlcpy(msg.topic,   topic,   sizeof(msg.topic));
    strlcpy(msg.payload, payload, sizeof(msg.payload));
    msg.retained = retained;
    bool ok = (xQueueSendToBack(xQueue_mqtt_tx, &msg, 0) == pdPASS);
    if (!ok) Serial.println(F("mqtt_tx_enqueue: TX queue full — message dropped"));
    return ok;
}

// ── Inbound message handlers ───────────────────────────────────────────────────

// OTA firmware now streams over the live MQTT link in chunks (see OTA.cpp,
// ota_mqtt_run()) — no HTTPS download, no second TLS session. The old URL-based
// HTTPS OTA (run_ota / ota_download_firmware) has been removed.

static void handle_rpc_message(const char* json) {
    StaticJsonDocument<1536> doc;  // 1536: room for a long presigned OTA URL in params
    if (deserializeJson(doc, json)) { Serial.println(F("RPC: JSON parse error")); return; }

    JsonObject data    = doc.containsKey("data") ? doc["data"].as<JsonObject>() : doc.as<JsonObject>();
    const char* method = data["method"];
    JsonObject  params = data["params"].as<JsonObject>();

    if (!data.containsKey("id") || method == nullptr) {
        Serial.println(F("RPC: ignored — missing id or method"));
        return;
    }
    int  reqId   = data["id"].as<int>();
    bool success = true;

    if (strcmp(method, "set_counter") == 0) {
        // Sets the lifetime production odometer only — a device-REPLACEMENT tool
        // (migrate a machine's accumulated count onto new hardware). Not for
        // normal operation: the session count resets via the manifest lifecycle
        // (R1), and ThingsBoard windows the odometer for daily/shift, so there is
        // no operational reason to poke the odometer otherwise.
        //   params {"value":<n>}
        // Single-writer: the odometer is read-modify-written by Task2, so we post
        // to the owner rather than write it here.
        int32_t val = params["value"] | -1;
        if (val < 0) {
            success = false;
            Serial.printf("set_counter: rejected — value=%ld\n", (long)val);
        } else {
            success = domain_cmd_post(SEW_CMD_SET_PRODUCTION, val);
            Serial.printf("set_counter: production=%ld -> %s\n",
                          (long)val, success ? "queued" : "QUEUE FULL");
        }

    } else if (strcmp(method, "get_tele") == 0) {
        // On-demand telemetry — the portal uses this to refresh a machine tile
        // without waiting for the next periodic publish. Task2 owns the data, so
        // it builds the frame; it lands on the next scan tick (~10 ms).
        success = domain_cmd_post(SEW_CMD_PUBLISH_NOW);
        Serial.printf("get_tele: %s\n", success ? "requested" : "QUEUE FULL");

    } else if (strcmp(method, "reload_config") == 0) {
        Serial.println(F("reload_config received"));

    } else if (strcmp(method, "reset_cfgindex") == 0) {
        // Zero stored cfgIndex version(s) and re-arm the fetch SM so the device
        // re-pulls from the server. params.key = a specific key, or omit for all.
        // Version-only (data files untouched) — pair with factory_default_* to
        // also drop to board defaults. No reboot.
        int reset = cfgindex_reset(params["key"].as<const char*>());
        success = (reset >= 0);
        if (success) {
            g_attrFetchState   = AFS_FETCH_META;
            g_attrRetryAfterMs = 0;
        }
        char res[80];
        snprintf(res, sizeof(res), "{\"reqId\":\"%d\",\"success\":%s,\"reset\":%d}",
                 reqId, success ? "true" : "false", reset);
        mqtt_tx_enqueue(AQ_TOPIC_RPC_RES, res, false);
        return;

    } else if (strcmp(method, "factory_reset") == 0) {
        // Wipe all config bins (keeps /wifi.json + /counter.txt) and reboot; the
        // next boot re-seeds board defaults and re-syncs from the server. Ack
        // directly since the reboot is imminent.
        int removed = config_factory_reset();
        char res[80];
        snprintf(res, sizeof(res), "{\"reqId\":\"%d\",\"success\":true,\"removed\":%d}", reqId, removed);
        publish_direct(AQ_TOPIC_RPC_RES, res, false);
        Serial.println(F("factory_reset: rebooting in 1s"));
        delay(1000);
        ESP.restart();
        return;

    } else if (strcmp(method, "device_restart") == 0) {
        // Plain remote reboot — recovery tool, no config touched. Ack directly
        // (not via the TX queue) since the reboot is imminent, then restart 1s
        // later so the ack actually leaves the radio. If an OTA was paused, the
        // restart aborts it and the next boot self-reports FAILED "aborted".
        char res[64];
        snprintf(res, sizeof(res), "{\"reqId\":\"%d\",\"success\":true}", reqId);
        publish_direct(AQ_TOPIC_RPC_RES, res, false);
        Serial.println(F("device_restart: rebooting in 1s"));
        delay(1000);
        ESP.restart();
        return;

    } else if (strcmp(method, "ota_mqtt") == 0 || strcmp(method, "ota_mqtt_fs") == 0) {
        // Chunked OTA over the live MQTT link — app firmware (ota_mqtt) or
        // filesystem image (ota_mqtt_fs), same transport, different partition.
        // ACK IMMEDIATELY — the two-way RPC caller times out at ~8 s, so we must
        // not hold it open for the transfer. We are in the inbound-queue drain
        // (NOT inside the PubSubClient callback), so ota_mqtt_run() may safely
        // pump client.loop() to fetch the chunks.
        bool is_fs = (strcmp(method, "ota_mqtt_fs") == 0);
        char res[96];
        if (ota_mqtt_active()) {
            // One OTA session globally (either type). A duplicate trigger must NOT
            // tear it down (that would kill a paused session about to resume).
            snprintf(res, sizeof(res), "{\"reqId\":\"%d\",\"success\":false,\"error\":\"ota_in_progress\"}", reqId);
            publish_direct(AQ_TOPIC_RPC_RES, res, false);
            Serial.println(F("OTA (mqtt): RPC ignored — an OTA is already running/paused"));
            return;
        }
        snprintf(res, sizeof(res), "{\"reqId\":\"%d\",\"success\":true,\"status\":\"started\"}", reqId);
        publish_direct(AQ_TOPIC_RPC_RES, res, false);
        Serial.printf("OTA (mqtt%s) via RPC — starting chunked download\n", is_fs ? ",fs" : "");
        ota_mqtt_run(is_fs);   // reboots on success; on failure it reports state:FAILED
        return;

    } else if (strcmp(method, "enter_portal") == 0) {
        // WiFi build edge case: this MQTT session rides on the WiFi link the
        // user is about to reconfigure. We persist the pending-portal flag,
        // ack the RPC, then reboot — the device drops out of MQTT during the
        // reboot and comes back up in portal AP mode where it serves the
        // setup page until new creds are saved (or the 10 min timeout fires).
        Serial.println(F("enter_portal RPC — flagging and rebooting"));
        wifi_creds_set_pending_portal(true);
        char ack[64];
        snprintf(ack, sizeof(ack), "{\"reqId\":\"%d\",\"success\":true}", reqId);
        publish_direct(AQ_TOPIC_RPC_RES, ack, false);
        delay(500);
        ESP.restart();

    } else {
        Serial.printf("RPC: unknown method '%s'\n", method);
        success = false;
    }

    char res[64];
    snprintf(res, sizeof(res), "{\"reqId\":\"%d\",\"success\":%s}",
             reqId, success ? "true" : "false");
    mqtt_tx_enqueue(AQ_TOPIC_RPC_RES, res, false);

    // Ask the data owner for a fresh frame right after the ack so the portal
    // reflects the new state without waiting out the interval. Task2 publishes.
    domain_cmd_post(SEW_CMD_PUBLISH_NOW);
}

static void handle_attr_response(const char* json) {
    StaticJsonDocument<64> probe;
    deserializeJson(probe, json);
    uint16_t res_id = probe["id"] | 0;

    if (g_attrFetchState != AFS_WAIT_META && g_attrFetchState != AFS_WAIT_KEY) {
        Serial.printf("attr/res: id=%u ignored — not waiting\n", res_id);
        return;
    }
    if (res_id != g_attrFetchWaitId) {
        Serial.printf("attr/res: id=%u ignored — want %u\n", res_id, g_attrFetchWaitId);
        return;
    }

    if (g_attrFetchState == AFS_WAIT_META) {
        char stale_keys[CFGINDEX_SLOTS][16];
        int n = cfgindex_process(json, stale_keys, CFGINDEX_SLOTS);
        Serial.printf("attr/res cfgIndex: %d stale key(s)\n", n);
        g_attrFetchState = AFS_FETCH_KEY;
    } else {
        Serial.printf("attr/res: applying key '%s'\n", g_attrFetchKey);
        cfgindex_save(g_attrFetchKey, json);   // routes sched_* vs config tables
        g_attrFetchState = AFS_FETCH_KEY;
    }
}

static void handle_attr_message(const char* json) {
    char stale_keys[CFGINDEX_MAX_BLOCKS][16];
    int n = cfgindex_process(json, stale_keys, CFGINDEX_MAX_BLOCKS);
    if (n > 0) {
        Serial.printf("attr/set: %d stale block(s) — starting fetch\n", n);
        if (g_attrFetchState == AFS_IDLE) g_attrFetchState = AFS_FETCH_KEY;
    } else {
        Serial.println(F("attr/set: all blocks up to date"));
        // Buffers are `static` (BSS, not stack) — same rationale as the GSM
        // transport: 6 KB on Task1's stack collides with the 8 KB
        // StaticJsonDocument inside cfgindex_process and blows the 20 KB
        // task stack. Only one Task1 path touches this, so single-instance
        // static buffers are safe and cheap.
        static char s_local_json[3072];
        static char s_attr_json[3100];
        cfgindex_get_local_json(s_local_json, sizeof(s_local_json));
        snprintf(s_attr_json, sizeof(s_attr_json), "{\"cfgIndex\":%s}", s_local_json);
        mqtt_tx_enqueue(AQ_TOPIC_ATTR_PUB, s_attr_json, false);
    }
}

// ── MQTT callback — dumb pipe ──────────────────────────────────────────────────
static void callback(char* topic, byte* payload, unsigned int length) {
    // Chunked-OTA replies (binary chunk/res + JSON meta/res) bypass the text
    // inbound queue — raw bytes, needed synchronously by ota_mqtt_run(). This is
    // just a memcpy+flag and only fires during an active OTA session.
    if (ota_mqtt_consume(topic, payload, length)) return;

    MqttInboundMsg msg = {};
    strlcpy(msg.topic, topic, sizeof(msg.topic));
    uint16_t len = (length < sizeof(msg.payload) - 1) ? length : sizeof(msg.payload) - 1;
    memcpy(msg.payload, payload, len);
    msg.payload[len] = '\0';
    if (xQueueSendToBack(xQueue_mqtt_inbound, &msg, 0) != pdPASS)
        Serial.printf("MQTT RX: inbound queue full — dropped [%s]\n", topic);
}

// ── Drain functions ────────────────────────────────────────────────────────────

static void drain_mqtt_inbound_queue() {
    if (!xQueue_mqtt_inbound) return;
    MqttInboundMsg msg = {};
    while (xQueueReceive(xQueue_mqtt_inbound, &msg, 0) == pdPASS) {
        Serial.printf("MQTT rx [%s]: %.120s\n", msg.topic, msg.payload);
        if      (strstr(msg.topic, "/rpc/req"))      handle_rpc_message(msg.payload);
        else if (strstr(msg.topic, "/attr/res"))     handle_attr_response(msg.payload);
        else if (strstr(msg.topic, "/attr/set"))     handle_attr_message(msg.payload);
        else Serial.printf("MQTT rx: unhandled topic '%s'\n", msg.topic);
    }
}

static void drain_mqtt_tx_queue() {
    if (!xQueue_mqtt_tx || !client.connected()) return;
    MqttTxMsg msg = {};
    while (xQueueReceive(xQueue_mqtt_tx, &msg, 0) == pdPASS) {
        publish_direct(msg.topic, msg.payload, msg.retained);
    }
}

// ── 5-state attribute fetch state machine ─────────────────────────────────────
static void tick_attr_fetch_sm() {
    if (!client.connected()) return;

    switch (g_attrFetchState) {

        case AFS_IDLE:
            if (g_attrRetryAfterMs && millis() >= g_attrRetryAfterMs) {
                g_attrRetryAfterMs = 0;
                g_attrFetchState   = AFS_FETCH_META;
            }
            return;

        case AFS_FETCH_META: {
            g_attrFetchWaitId = ++g_attrFetchReqId;
            char req[64];
            snprintf(req, sizeof(req), "{\"id\":%u,\"key\":\"cfgIndex\"}", g_attrFetchWaitId);
            publish_direct(AQ_TOPIC_ATTR_REQUEST, req, false);
            g_attrFetchLastMs = millis();
            g_attrFetchState  = AFS_WAIT_META;
            Serial.printf("attr fetch: requesting cfgIndex (id=%u)\n", g_attrFetchWaitId);
            return;
        }

        case AFS_WAIT_META:
            if (millis() - g_attrFetchLastMs > kAttrFetchTimeoutMs) {
                Serial.println(F("attr fetch: cfgIndex timeout — will retry"));
                g_attrFetchState   = AFS_IDLE;
                g_attrRetryAfterMs = millis() + kAttrFetchRetryIntervalMs;
            }
            return;

        case AFS_FETCH_KEY: {
            char key[16] = {};
            if (!cfgindex_next_pending(key)) {
                Serial.println(F("attr fetch: all blocks synced"));
                // static (BSS) buffers — same rationale as handle_attr_message
                // above. Both publish sites share the same buffer pair (single-
                // threaded Task1, so one instance is enough).
                static char s_local_json[3072];
                static char s_attr_json[3100];
                cfgindex_get_local_json(s_local_json, sizeof(s_local_json));
                snprintf(s_attr_json, sizeof(s_attr_json), "{\"cfgIndex\":%s}", s_local_json);
                mqtt_tx_enqueue(AQ_TOPIC_ATTR_PUB, s_attr_json, false);
                g_attrFetchState   = AFS_IDLE;
                g_attrRetryAfterMs = millis() + kAttrFetchRetryIntervalMs;
                return;
            }
            strlcpy(g_attrFetchKey, key, sizeof(g_attrFetchKey));
            g_attrFetchKeyRetries = 0;
            g_attrFetchWaitId     = ++g_attrFetchReqId;
            char req[64];
            snprintf(req, sizeof(req), "{\"id\":%u,\"key\":\"%s\"}", g_attrFetchWaitId, key);
            publish_direct(AQ_TOPIC_ATTR_REQUEST, req, false);
            g_attrFetchLastMs = millis();
            g_attrFetchState  = AFS_WAIT_KEY;
            Serial.printf("attr fetch: requesting block '%s' (id=%u)\n", key, g_attrFetchWaitId);
            return;
        }

        case AFS_WAIT_KEY:
            if (millis() - g_attrFetchLastMs > kAttrFetchTimeoutMs) {
                if (g_attrFetchKeyRetries < kAttrFetchMaxRetries) {
                    ++g_attrFetchKeyRetries;
                    char req[64];
                    snprintf(req, sizeof(req), "{\"id\":%u,\"key\":\"%s\"}",
                             g_attrFetchWaitId, g_attrFetchKey);
                    publish_direct(AQ_TOPIC_ATTR_REQUEST, req, false);
                    g_attrFetchLastMs = millis();
                    Serial.printf("attr fetch: timeout '%s' retry %u/%u\n",
                        g_attrFetchKey, g_attrFetchKeyRetries, kAttrFetchMaxRetries);
                } else {
                    Serial.printf("attr fetch: gave up on '%s'\n", g_attrFetchKey);
                    cfgindex_clear_pending(g_attrFetchKey);
                    g_attrFetchState = AFS_FETCH_KEY;
                }
            }
            return;
    }
}

// ── WiFi event handlers ────────────────────────────────────────────────────────

static void onWifiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println(F("WiFi IP acquired — MQTT will connect via com_loop()"));
    wifi_ready = true;
}

static void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println(F("WiFi disconnected — waiting for reconnection"));
    wifi_ready = false;
}

// ── MQTT connect ───────────────────────────────────────────────────────────────

static void connectMQTT() {
    char clientId[32];
    snprintf(clientId, sizeof(clientId), AQ_TOPIC_ROOT "_%s", AQ_DEVICE_MAC);
    Serial.printf("Connecting MQTT (WiFi) as %s\n", clientId);
    // Connectivity is shown by the status LEDs (led_status.cpp) via g_mqtt_online.
    g_mqtt_online = false;   // we only get here while disconnected

    if (client.connect(clientId, mqttUser, mqttPassword)) {
        Serial.println(F("MQTT connected via WiFi"));
        g_mqtt_online = true;   // status LED [3] → green

        client.subscribe(AQ_TOPIC_RPC_REQ);
        client.subscribe(AQ_TOPIC_ATTR_SET);
        client.subscribe(AQ_TOPIC_ATTR_RES);
        // OTA subscribes its own ota/chunk/res + meta/res topics only during a
        // transfer (see ota_mqtt_run) — nothing to sub here.
        Serial.printf("Subscribed: %s\n", AQ_TOPIC_RPC_REQ);
        Serial.printf("Subscribed: %s\n", AQ_TOPIC_ATTR_SET);
        Serial.printf("Subscribed: %s\n", AQ_TOPIC_ATTR_RES);

        publish_info();

        // Report the outcome of a prior OTA now, on a fresh connection.
        {
            // Report the outcome of a prior OTA on the subtree the flash
            // belonged to (app -> ota/state, filesystem -> otafs/state).
            bool was_fs = false; char fsver[24] = {0};
            char topic[AQ_TOPIC_LEN]; char st[96];
            if (ota_boot_take_success(&was_fs, fsver, sizeof(fsver))) {
                if (was_fs) {
                    // filesystem flash — fw_ver unchanged, echo the fs label
                    snprintf(topic, sizeof(topic), AQ_TOPIC_ROOT "/%s/otafs/state", AQ_DEVICE_MAC);
                    snprintf(st, sizeof(st), "{\"state\":\"UPDATED\",\"fs_version\":\"%s\"}", fsver);
                } else {
                    // app flash — lets the backend clear TB's "update pending"
                    snprintf(topic, sizeof(topic), AQ_TOPIC_ROOT "/%s/ota/state", AQ_DEVICE_MAC);
                    snprintf(st, sizeof(st), "{\"state\":\"UPDATED\",\"current_fw_version\":\"%s\"}", FW_VER);
                }
                mqtt_tx_enqueue(topic, st, false);
            } else if (ota_boot_take_aborted(&was_fs)) {
                // interrupted mid-download — unstick the stuck DOWNLOADING state
                snprintf(topic, sizeof(topic), AQ_TOPIC_ROOT "/%s/%s/state",
                         AQ_DEVICE_MAC, was_fs ? "otafs" : "ota");
                mqtt_tx_enqueue(topic, "{\"state\":\"FAILED\",\"err\":\"aborted\"}", false);
            }
        }

        // Kick the attr fetch SM — it will request cfgIndex on the next tick
        g_attrFetchState   = AFS_FETCH_META;
        g_attrRetryAfterMs = 0;
    } else {
        Serial.printf("MQTT connect failed — state=%d, retry next loop\n", client.state());
    }
}

// ── Public API ─────────────────────────────────────────────────────────────────

void mqtt_setup() {
    xQueue_mqtt_tx      = xQueueCreate(MQTT_TX_QUEUE_DEPTH, sizeof(MqttTxMsg));
    xQueue_mqtt_inbound = xQueueCreate(MQTT_RX_QUEUE_DEPTH, sizeof(MqttInboundMsg));

    wifiSecureClient.setCACert(ca_cert);
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);

    WiFi.onEvent(onWifiGotIP,        WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWifiDisconnected,  WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    Serial.println(F("WiFi MQTT transport ready"));
}

void com_loop() {
    if (!wifi_ready) return;
    if (!client.connected()) { connectMQTT(); return; }   // reconnect (also recovers a paused OTA)

    client.loop();
    if (ota_mqtt_pending()) ota_mqtt_resume();   // continue a stalled OTA now the link is back
    drain_mqtt_inbound_queue();
    drain_mqtt_tx_queue();
    tick_attr_fetch_sm();
    // Telemetry is NOT paced here — Task2 owns the counters and publishes them
    // through the TX queue (see sewing_tele.cpp). This task only drains.
}

void publish_info() {
    // Static identity + capability, published once per connect as a client
    // attribute. `inputs` is the number of monitored machine lines this board
    // scans (sensor_pin_count), the sewing analogue of PrimeHive's relay count.
    char buf[240];
    snprintf(buf, sizeof(buf),
        "{\"hw_caps\":{\"model\":\"" DEFAULT_BOARD_MODEL "\",\"inputs\":%u,\"fw_ver\":\"" FW_VER "\""
        ",\"friendly_name\":\"%s\",\"location\":\"%s\"}}",
        (unsigned)sensor_pin_count, structSysConfig.friendly_name, structSysConfig.location);
    mqtt_tx_enqueue(AQ_TOPIC_ATTR_PUB, buf, false);
}

bool send_mqtt_message(const char* topic, const char* message, bool retained) {
    return mqtt_tx_enqueue(topic, message, retained);
}

#endif // WLAN
