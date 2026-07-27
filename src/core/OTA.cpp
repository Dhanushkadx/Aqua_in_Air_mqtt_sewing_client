#include "OTA.h"
#include <PubSubClient.h>
#include <Preferences.h>        // NVS success marker (reported after the reboot)
#include <ArduinoJson.h>        // parse meta/res
#include <mbedtls/sha256.h>     // streaming whole-image hash
#include <strings.h>            // strcasecmp
#include <LittleFS.h>           // unmounted before a filesystem-image flash
#include <esp_partition.h>      // fs partition capacity for the pre-flight size check
#include "aquasew_topics.h"   // AQ_DEVICE_MAC
#include "led_status.h"         // led_status_set_ota — freeze the strip during OTA
#include "../pinsx.h"              // FW_VER (reported after a successful OTA)

// The MQTT client lives in whichever transport got compiled (gprs_mqtt.cpp or
// wifi_mqtt.cpp); both expose it as `extern PubSubClient client` so the OTA path
// can publish/subscribe + pump it without a transport-specific header.
extern PubSubClient client;

// ── RainmakerOTA: chunked firmware OTA over the LIVE MQTT connection ──────────
// Instead of a second HTTPS/TLS session (which never fit the ~40 KB contiguous
// heap on this WiFi+GSM board), the whole image is pulled in small chunks over
// the MQTT link that is already open. Node-RED serves the bytes from the
// ThingsBoard OTA store. Protocol (see C:\claudeDoc\change_request.txt):
//   device  -> aquasew/<MAC>/ota/meta/req           {"reqId":n}
//   NodeRED -> aquasew/<MAC>/ota/meta/res           {"version","size","sha256","chunkSize"}
//   device  -> aquasew/<MAC>/ota/chunk/req          {"reqId":n,"offset":o,"len":l}
//   NodeRED -> aquasew/<MAC>/ota/chunk/res/<offset> RAW BINARY bytes [o..o+l)
//   device  -> aquasew/<MAC>/ota/state              {"state":...}
// The <offset> is in the chunk/res TOPIC (not the payload) so we can reject a
// stale / duplicate / retried chunk (QoS1) — we only accept the chunk whose
// <offset> matches the offset we are currently waiting for.

// ── Tunables ─────────────────────────────────────────────────────────────────
static const uint32_t OTA_META_TIMEOUT_MS  = 8000;   // wait for meta/res
static const uint8_t  OTA_META_RETRIES     = 3;
static const uint32_t OTA_CHUNK_TIMEOUT_MS = 10000;   // wait for one chunk (in-place)
static const uint8_t  OTA_CHUNK_RETRIES    = 3;      // in-place re-requests before a PAUSE
static const uint8_t  OTA_MAX_PAUSES       = 10;     // pause/resume cycles before giving up —
                                                     // each pause is a reconnect + re-meta, and
                                                     // stays well under Node-RED's 20-min buffer TTL
static const uint16_t OTA_CHUNK_BUF_MAX    = 1600;   // >= negotiated chunkSize (1536)

// ── Session state (single OTA at a time; all runs on the MQTT task) ───────────
static volatile bool s_active = false;

static volatile bool s_meta_ready = false;
static char          s_meta_json[192];

static volatile bool s_chunk_ready = false;
static uint32_t      s_chunk_off   = 0;
static uint16_t      s_chunk_len   = 0;
static uint8_t       s_chunk_buf[OTA_CHUNK_BUF_MAX];

// Response topics matched in ota_mqtt_consume() (built at session start).
// The session is parameterized by image type: app firmware uses the "ota/"
// subtree + U_FLASH, filesystem uses "otafs/" + U_SPIFFS — same transport.
static char    s_topic_meta_res[AQ_TOPIC_LEN];        // .../ota[fs]/meta/res
static char    s_topic_chunk_res_pfx[AQ_TOPIC_LEN];   // .../ota[fs]/chunk/res/
static uint8_t s_topic_chunk_res_pfx_len = 0;
// Request/subscribe/state topics (also reused on resume).
static char    s_topic_meta_req[AQ_TOPIC_LEN];        // .../ota[fs]/meta/req
static char    s_topic_chunk_req[AQ_TOPIC_LEN];       // .../ota[fs]/chunk/req
static char    s_topic_chunk_sub[AQ_TOPIC_LEN];       // .../ota[fs]/chunk/res/+
static char    s_topic_state[AQ_TOPIC_LEN];           // .../ota[fs]/state
static bool    s_is_fs        = false;   // this session flashes the filesystem image
static bool    s_fs_unmounted = false;   // LittleFS.end() done — past this a failed
                                         // fs session must reboot to recover the FS

// ── Resumable session state (persists across a PAUSE/return) ──────────────────
// On a stalled chunk we PAUSE: keep the open Update session, the running SHA-256,
// and the byte offset, return, let com_loop reconnect, then ota_mqtt_resume()
// re-subscribes, re-sends meta/req (re-arms Node-RED's cached buffer per RQ3) and
// continues chunk/req from s_offset. No re-download; survives a mid-download drop.
static uint32_t s_size      = 0;         // total image bytes (from meta)
static uint32_t s_chunkSize = 1536;      // negotiated chunk size
static char     s_sha_expect[72] = {0};  // expected whole-image sha256 (hex)
static char     s_version[24]    = {0};  // TB package version label
static uint32_t s_offset    = 0;         // bytes written + hashed so far
static mbedtls_sha256_context s_sha;     // running hash — kept alive across pauses
static bool     s_paused      = false;   // true while waiting for a reconnect to resume
static uint8_t  s_pause_count = 0;       // pause budget consumed

// ── Inbound hook — called from callback() for every message ───────────────────
// Runs on the same task that pumps client.loop() inside ota_mqtt_run(), so the
// copy-into-buffer + flag is a plain single-threaded store (no lock needed).
bool ota_mqtt_consume(const char* topic, const uint8_t* payload, unsigned int len) {
    if (!s_active) return false;

    // meta/res — small JSON, copy as a string
    if (strcmp(topic, s_topic_meta_res) == 0) {
        unsigned n = (len < sizeof(s_meta_json) - 1) ? len : sizeof(s_meta_json) - 1;
        memcpy(s_meta_json, payload, n);
        s_meta_json[n] = '\0';
        s_meta_ready = true;
        return true;
    }

    // chunk/res/<offset> — raw binary; parse the offset out of the topic tail
    if (s_topic_chunk_res_pfx_len &&
        strncmp(topic, s_topic_chunk_res_pfx, s_topic_chunk_res_pfx_len) == 0) {
        s_chunk_off = strtoul(topic + s_topic_chunk_res_pfx_len, nullptr, 10);
        uint16_t n  = (len < sizeof(s_chunk_buf)) ? (uint16_t)len : (uint16_t)sizeof(s_chunk_buf);
        memcpy(s_chunk_buf, payload, n);
        s_chunk_len   = n;
        s_chunk_ready = true;
        return true;
    }
    return false;
}

// ── Small helpers ─────────────────────────────────────────────────────────────
static void ota_publish_state(const char* json) {
    client.publish(s_topic_state, json);   // ota/state or otafs/state per session
}

static void hex32(const uint8_t* d, char* out) {   // 32 bytes -> 64 lowercase hex + NUL
    static const char* h = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out[i*2] = h[d[i] >> 4]; out[i*2 + 1] = h[d[i] & 0xF]; }
    out[64] = '\0';
}

// "OTA in progress" NVS marker. Set once we start writing flash; a clean failure
// clears it, and a successful reboot clears it in ota_boot_check(). If it survives
// to the next boot it means the OTA was interrupted (reset / power-loss / crash),
// so we report FAILED "aborted" to unstick the server. The "fs" flag records
// WHICH image was being flashed so the boot-time report goes to the right
// subtree (ota/state vs otafs/state).
static void ota_set_inprogress(bool v) {
    Preferences p;
    if (p.begin("ota", false)) {
        p.putBool("inprog", v);
        if (v) p.putBool("fs", s_is_fs);
        p.end();
    }
}

// ── Session helpers ───────────────────────────────────────────────────────────
static void ota_build_topics(bool fs) {
    const char* mac = AQ_DEVICE_MAC;
    const char* t   = fs ? "otafs" : "ota";   // parallel subtree per image type
    snprintf(s_topic_meta_res,      sizeof(s_topic_meta_res),      AQ_TOPIC_ROOT "/%s/%s/meta/res",    mac, t);
    snprintf(s_topic_meta_req,      sizeof(s_topic_meta_req),      AQ_TOPIC_ROOT "/%s/%s/meta/req",    mac, t);
    snprintf(s_topic_chunk_req,     sizeof(s_topic_chunk_req),     AQ_TOPIC_ROOT "/%s/%s/chunk/req",   mac, t);
    snprintf(s_topic_chunk_sub,     sizeof(s_topic_chunk_sub),     AQ_TOPIC_ROOT "/%s/%s/chunk/res/+", mac, t);
    snprintf(s_topic_chunk_res_pfx, sizeof(s_topic_chunk_res_pfx), AQ_TOPIC_ROOT "/%s/%s/chunk/res/",  mac, t);
    snprintf(s_topic_state,         sizeof(s_topic_state),         AQ_TOPIC_ROOT "/%s/%s/state",       mac, t);
    s_topic_chunk_res_pfx_len = strlen(s_topic_chunk_res_pfx);
}

static void ota_subscribe() {
    client.subscribe(s_topic_meta_res, 1);
    client.subscribe(s_topic_chunk_sub, 1);   // QoS1, per the agreed protocol
}

// Do a meta/req -> meta/res handshake. On success fills the out params and
// returns true; false on timeout / parse error after all retries. `target`
// receives the optional "target" echo from meta/res (empty string if absent) —
// the fs path checks it against "spiffs" as a wrong-image guard.
static bool ota_fetch_meta(uint32_t& reqId, uint32_t* size, uint32_t* chunkSize,
                           char* sha, size_t shalen, char* ver, size_t verlen,
                           char* target, size_t tgtlen) {
    for (uint8_t a = 0; a < OTA_META_RETRIES; a++) {
        s_meta_ready = false;
        char req[48];
        snprintf(req, sizeof(req), "{\"reqId\":%lu}", (unsigned long)++reqId);
        client.publish(s_topic_meta_req, req);
        uint32_t t0 = millis();
        while ((uint32_t)(millis() - t0) < OTA_META_TIMEOUT_MS && !s_meta_ready) {
            client.loop();
            delay(2);
        }
        if (!s_meta_ready) continue;
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, s_meta_json)) continue;
        uint32_t    sz = doc["size"]      | 0UL;
        uint32_t    cs = doc["chunkSize"] | 1536U;
        const char* sh = doc["sha256"]    | "";
        if (sz > 0 && strlen(sh) == 64 && cs > 0 && cs <= sizeof(s_chunk_buf)) {
            *size = sz; *chunkSize = cs;
            strlcpy(sha, sh, shalen);
            strlcpy(ver, doc["version"] | "", verlen);
            strlcpy(target, doc["target"] | "", tgtlen);
            return true;
        }
    }
    return false;
}

// Pull chunks from s_offset up to s_size, writing + hashing each. Returns
//   OTA_OK      — the whole image is written (caller verifies + finalizes)
//   OTA_PAUSED  — stalled; session state preserved, resume after a reconnect
//   OTA_ERR_*   — a hard failure (caller reports FAILED + aborts)
static OtaResult ota_pull(uint32_t& reqId) {
    int last_pct = -1;
    while (s_offset < s_size) {
        uint16_t want = (uint16_t)((s_size - s_offset < s_chunkSize) ? (s_size - s_offset) : s_chunkSize);
        bool got = false;
        for (uint8_t a = 0; a < OTA_CHUNK_RETRIES && !got; a++) {
            s_chunk_ready = false;
            char req[80];
            snprintf(req, sizeof(req), "{\"reqId\":%lu,\"offset\":%lu,\"len\":%u}",
                     (unsigned long)++reqId, (unsigned long)s_offset, want);
            client.publish(s_topic_chunk_req, req);
            uint32_t t0 = millis();
            while ((uint32_t)(millis() - t0) < OTA_CHUNK_TIMEOUT_MS) {
                client.loop();                // may fire callback() -> ota_mqtt_consume()
                if (s_chunk_ready) {
                    if (s_chunk_off == s_offset) { got = true; break; }
                    s_chunk_ready = false;    // stale / duplicate / retry for another offset — ignore
                }
                if (!client.connected()) break;   // link dropped — stop waiting, go pause fast
                delay(2);
            }
            if (!got && !client.connected()) break;   // don't burn the remaining retries while offline
        }
        if (!got) {
            // Stalled. PAUSE (keep flash session + hash + offset) and let com_loop
            // reconnect, unless we've burned the whole pause budget.
            if (++s_pause_count > OTA_MAX_PAUSES) return OTA_ERR_TIMEOUT;
            s_paused = true;
            Serial.printf("OTA(mqtt): stalled at %lu — pausing (%u/%u), resume on reconnect\n",
                          (unsigned long)s_offset, s_pause_count, OTA_MAX_PAUSES);
            return OTA_PAUSED;
        }
        if (Update.write(s_chunk_buf, s_chunk_len) != s_chunk_len) return OTA_ERR_WRITE;
        mbedtls_sha256_update(&s_sha, s_chunk_buf, s_chunk_len);
        s_offset += s_chunk_len;

        int pct = (int)((uint64_t)s_offset * 100 / s_size);
        if (pct != last_pct) {
            last_pct = pct;
            char st[72];
            snprintf(st, sizeof(st), "{\"state\":\"DOWNLOADING\",\"offset\":%lu,\"size\":%lu}",
                     (unsigned long)s_offset, (unsigned long)s_size);
            ota_publish_state(st);
            Serial.printf("OTA(mqtt): %d%% (%lu/%lu)\n", pct, (unsigned long)s_offset, (unsigned long)s_size);
        }
    }
    return OTA_OK;
}

// Whole-image hash check + Update.end. Frees the hash context.
static OtaResult ota_finalize() {
    uint8_t digest[32]; char sha_calc[68];
    mbedtls_sha256_finish(&s_sha, digest);
    mbedtls_sha256_free(&s_sha);
    hex32(digest, sha_calc);
    if (strcasecmp(sha_calc, s_sha_expect) != 0) {
        Serial.printf("OTA(mqtt): sha mismatch calc=%s exp=%s\n", sha_calc, s_sha_expect);
        return OTA_ERR_SHA;
    }
    if (!Update.end()) {
        Serial.printf("OTA(mqtt): end failed: %s\n", Update.errorString());
        return OTA_ERR_END;
    }
    return OTA_OK;
}

// Success — publish VERIFIED, mark the NVS success marker, reboot (no return).
static void ota_succeed() {
    char st[48];
    snprintf(st, sizeof(st), "{\"state\":\"VERIFIED\",\"version\":\"%s\"}", s_version);
    ota_publish_state(st);
    Serial.println(F("OTA(mqtt): success — rebooting"));
    ota_mark_boot_success();                 // -> state:UPDATED on the next boot
    delay(400);                              // let VERIFIED flush before we drop the link
    ESP.restart();
}

// Hard failure — publish FAILED (MQTT is up), free the hash, abort the flash,
// and tear the session down.
static void ota_fail(OtaResult r) {
    char code[24];
    ota_error_code(r, code, sizeof(code));
    char st[72];
    // key is "err" — Node-RED maps err -> fw_error / fs_error on the TB side.
    snprintf(st, sizeof(st), "{\"state\":\"FAILED\",\"err\":\"%s\"}", code);
    ota_publish_state(st);
    Serial.printf("OTA(mqtt): failed — %s\n", code);

    mbedtls_sha256_free(&s_sha);             // harmless if already freed in ota_finalize()
    Update.abort();
    ota_set_inprogress(false);               // reported terminal FAILED — not an abort
    client.unsubscribe(s_topic_meta_res);
    client.unsubscribe(s_topic_chunk_sub);
    led_status_set_ota(false);
    s_topic_chunk_res_pfx_len = 0;
    s_paused = false;
    s_active = false;

    // A failed FILESYSTEM flash after LittleFS was unmounted can't just carry
    // on: the partition now holds a half-written image and the FS is offline
    // (config/counter writes from other tasks are failing). Reboot — mount at
    // boot auto-formats if needed and reseeds board defaults, restoring a
    // working filesystem. (FAILED was already published + flushed above.)
    if (s_is_fs && s_fs_unmounted) {
        Serial.println(F("OTA(mqtt): fs flash failed after unmount — rebooting to recover FS"));
        delay(400);
        ESP.restart();
    }
}

// Drive whatever pull() returned to its outcome (shared by run + resume).
static OtaResult ota_settle(OtaResult r) {
    if (r == OTA_PAUSED) return OTA_PAUSED;          // keep the session; com_loop resumes
    if (r != OTA_OK)    { ota_fail(r); return r; }
    r = ota_finalize();
    if (r != OTA_OK)    { ota_fail(r); return r; }
    ota_succeed();                                    // reboots — not reached
    return OTA_OK;
}

// ── Public: start a fresh OTA (from the ota_mqtt / ota_mqtt_fs RPC drain) ─────
// is_fs=false: app firmware -> "ota/" subtree, U_FLASH.
// is_fs=true : filesystem   -> "otafs/" subtree, U_SPIFFS (LittleFS image).
OtaResult ota_mqtt_run(bool is_fs) {
    if (s_active) { Update.abort(); mbedtls_sha256_free(&s_sha); }   // tear down a stale session
    s_is_fs        = is_fs;
    s_fs_unmounted = false;
    ota_build_topics(is_fs);
    s_active      = true;
    s_paused      = false;
    s_pause_count = 0;
    s_offset      = 0;
    led_status_set_ota(true);        // freeze the LED strip — show() IRQ-off would corrupt the link mid-download
    ota_subscribe();

    uint32_t reqId = millis();
    char target[12] = {0};
    if (!ota_fetch_meta(reqId, &s_size, &s_chunkSize, s_sha_expect, sizeof(s_sha_expect),
                        s_version, sizeof(s_version), target, sizeof(target))) {
        ota_fail(OTA_ERR_META);
        return OTA_ERR_META;
    }
    Serial.printf("OTA(mqtt%s): size=%lu chunk=%lu ver=%s\n", is_fs ? ",fs" : "",
                  (unsigned long)s_size, (unsigned long)s_chunkSize, s_version);

    if (is_fs) {
        // Wrong-image guard: the optional "target" echo in meta/res must say
        // "spiffs" on the fs tree (absent = accepted, for compatibility). The
        // sha256 can't catch an app image served to the fs path — it would
        // match itself — so this is the only cheap cross-check.
        if (target[0] != '\0' && strcmp(target, "spiffs") != 0) {
            Serial.printf("OTA(mqtt,fs): meta target '%s' != spiffs — wrong image\n", target);
            ota_fail(OTA_ERR_TARGET);
            return OTA_ERR_TARGET;
        }
        // Pre-flight size check against the real fs partition (resolved at
        // runtime, not hardcoded) BEFORE unmounting anything.
        const esp_partition_t* part = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
        uint32_t cap = part ? part->size : 0;
        if (s_size > cap) {
            Serial.printf("OTA(mqtt,fs): image %lu > fs partition %lu\n",
                          (unsigned long)s_size, (unsigned long)cap);
            ota_fail(OTA_ERR_FS_SIZE);
            return OTA_ERR_FS_SIZE;
        }
        // Unmount so no task writes the partition underneath the flash session
        // (counter/config saves just fail a file-open and log until reboot).
        LittleFS.end();
        s_fs_unmounted = true;
    }

    if (!Update.begin(s_size, is_fs ? U_SPIFFS : U_FLASH)) {
        Serial.printf("OTA(mqtt%s): begin failed: %s\n", is_fs ? ",fs" : "", Update.errorString());
        ota_fail(OTA_ERR_BEGIN);
        return OTA_ERR_BEGIN;
    }
    ota_set_inprogress(true);        // flash is being written — next boot reports abort if we die here
    mbedtls_sha256_init(&s_sha);
    mbedtls_sha256_starts(&s_sha, 0);

    return ota_settle(ota_pull(reqId));
}

// ── Public: resume a paused OTA (from com_loop after a reconnect) ─────────────
OtaResult ota_mqtt_resume() {
    if (!(s_active && s_paused)) return OTA_OK;
    s_paused = false;
    led_status_set_ota(true);        // stay frozen
    ota_subscribe();                 // the broker session may have reset — re-subscribe

    // Re-arm Node-RED's cached buffer (RQ3/RQ5: always re-meta before resuming)
    // and re-verify the package didn't change under us.
    uint32_t reqId = millis();
    uint32_t size = 0, chunkSize = 0;
    char sha[72] = {0}, ver[24] = {0}, target[12] = {0};
    if (!ota_fetch_meta(reqId, &size, &chunkSize, sha, sizeof(sha), ver, sizeof(ver),
                        target, sizeof(target))) {
        if (++s_pause_count > OTA_MAX_PAUSES) { ota_fail(OTA_ERR_TIMEOUT); return OTA_ERR_TIMEOUT; }
        s_paused = true;             // couldn't re-arm yet — stay paused, try again next reconnect
        return OTA_PAUSED;
    }
    if (size != s_size || strcasecmp(sha, s_sha_expect) != 0) {
        Serial.println(F("OTA(mqtt): package changed mid-OTA — aborting"));
        ota_fail(OTA_ERR_SHA);
        return OTA_ERR_SHA;
    }
    Serial.printf("OTA(mqtt): resuming at %lu/%lu\n", (unsigned long)s_offset, (unsigned long)s_size);

    return ota_settle(ota_pull(reqId));
}

// True while an OTA is paused waiting for the link to come back — com_loop polls
// this after (re)connecting and calls ota_mqtt_resume().
bool ota_mqtt_pending() {
    return s_active && s_paused;
}

// True while an OTA session exists (running OR paused). The ota_mqtt RPC handler
// checks this and rejects a duplicate trigger, so a re-sent RPC can't tear down a
// paused session that is about to resume.
bool ota_mqtt_active() {
    return s_active;
}

// ── Error-code mapping ────────────────────────────────────────────────────────
void ota_error_code(OtaResult res, char* out, size_t len) {
    switch (res) {
        case OTA_OK:          strlcpy(out, "ok",                 len); break;
        case OTA_PAUSED:      strlcpy(out, "paused",             len); break;
        case OTA_ERR_META:    strlcpy(out, "meta_failed",        len); break;
        case OTA_ERR_BEGIN:   strlcpy(out, "flash_begin_failed", len); break;
        case OTA_ERR_TIMEOUT: strlcpy(out, "chunk_timeout",      len); break;
        case OTA_ERR_WRITE:   strlcpy(out, "write_failed",       len); break;
        case OTA_ERR_SHA:     strlcpy(out, "sha_mismatch",       len); break;
        case OTA_ERR_END:     strlcpy(out, "verify_failed",      len); break;
        case OTA_ERR_FS_SIZE: strlcpy(out, "fs_too_big",         len); break;
        case OTA_ERR_TARGET:  strlcpy(out, "target_mismatch",    len); break;
        default:              strlcpy(out, "unknown",            len); break;
    }
}

// ── Success / abort across the reboot ────────────────────────────────────────
static bool s_booted_after_ota = false;   // last boot followed a good flash
static bool s_booted_aborted   = false;   // last boot followed an interrupted OTA
static bool s_boot_was_fs      = false;   // ...and it was the FILESYSTEM image
static char s_boot_fsver[24]   = {0};     // fs version label persisted pre-reboot

// Persist the outcome markers in NVS just before rebooting on a good flash, so
// the next boot can report UPDATED on the right subtree (fw_ver alone can't
// confirm a same-version app flash, and an fs flash changes no version at all —
// so the fs meta version label is persisted here to echo in UPDATED).
void ota_mark_boot_success() {
    Preferences p;
    if (p.begin("ota", false)) {
        p.putBool("ok", true);
        p.putBool("fs", s_is_fs);
        if (s_is_fs) p.putString("fsver", s_version);
        p.end();
    }
}

// On boot: inspect the NVS markers and latch the outcome in RAM. Called from
// setup(). "ok" wins (a good flash that also had inprog set is a success, not an
// abort); a lingering "inprog" without "ok" means the OTA was interrupted.
void ota_boot_check() {
    Preferences p;
    if (!p.begin("ota", false)) return;
    bool ok  = p.getBool("ok", false);
    bool inp = p.getBool("inprog", false);
    if (ok || inp) {
        s_boot_was_fs = p.getBool("fs", false);
        if (s_boot_was_fs) p.getString("fsver", s_boot_fsver, sizeof(s_boot_fsver));
    }
    if (ok) {
        p.putBool("ok", false);
        p.putBool("inprog", false);
        s_booted_after_ota = true;
        Serial.printf("OTA: booted after a successful %s flash — will report UPDATED\n",
                      s_boot_was_fs ? "filesystem" : "firmware");
    } else if (inp) {
        p.putBool("inprog", false);
        s_booted_aborted = true;
        Serial.printf("OTA: booted after an interrupted %s OTA — will report FAILED aborted\n",
                      s_boot_was_fs ? "filesystem" : "firmware");
    }
    p.end();
}

// The transport calls these once it has (re)connected to MQTT after the reboot,
// so the outcome is published on a fresh, live connection (right subtree per
// *was_fs; fs success also hands back the persisted version label).
bool ota_boot_take_success(bool* was_fs, char* fsver_out, size_t len) {
    if (!s_booted_after_ota) return false;
    s_booted_after_ota = false;
    if (was_fs)   *was_fs = s_boot_was_fs;
    if (fsver_out) strlcpy(fsver_out, s_boot_fsver, len);
    return true;
}

bool ota_boot_take_aborted(bool* was_fs) {
    if (!s_booted_aborted) return false;
    s_booted_aborted = false;
    if (was_fs) *was_fs = s_boot_was_fs;
    return true;
}
