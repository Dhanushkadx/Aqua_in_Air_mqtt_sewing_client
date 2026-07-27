// guard-band header
#ifndef OTA_H
#define OTA_H

#include "Arduino.h"
#include <Update.h>

// ── OTA outcome ───────────────────────────────────────────────────────────────
// ota_mqtt_run() returns one of these. On OTA_OK it does NOT return (it reboots);
// any other value is a failure that was already reported on the ota/state topic.
// ota_error_code() maps a result to the short "fw_error" string the backend sees.
enum OtaResult {
    OTA_OK = 0,          // flashed & verified — device reboots
    OTA_PAUSED,          // stalled mid-download; session preserved, resume after reconnect
    OTA_ERR_META,        // no / invalid meta from Node-RED
    OTA_ERR_BEGIN,       // Update.begin failed (partition / heap)
    OTA_ERR_TIMEOUT,     // a chunk never arrived after the pause budget was exhausted
    OTA_ERR_WRITE,       // Update.write failed / short write
    OTA_ERR_SHA,         // whole-image sha256 mismatch
    OTA_ERR_END,         // Update.end failed (finalize/verify)
    OTA_ERR_FS_SIZE,     // fs image bigger than the fs partition ("fs_too_big")
    OTA_ERR_TARGET,      // meta "target" echo != "spiffs" on the fs tree ("target_mismatch")
};

// Fill `out` (>= 24 bytes) with the backend-facing code, e.g. "sha_mismatch".
void ota_error_code(OtaResult res, char* out, size_t len);

// ── Chunked MQTT OTA (RainmakerOTA transport) ────────────────────────────────
// The whole firmware is pulled in chunks over the LIVE MQTT connection — no
// second TLS session, no WiFi — so the ~40 KB contiguous-heap ceiling on this
// WiFi+GSM board stops mattering. Node-RED serves the bytes from the
// ThingsBoard OTA store. Full protocol: C:\claudeDoc\change_request.txt.

// Feed EVERY inbound MQTT message here from callback() BEFORE the normal queue
// push. During an active OTA it consumes the raw-binary chunk/res and the JSON
// meta/res replies (they must bypass the text inbound queue) and returns true;
// otherwise it returns false (and always false when no OTA is running), so it is
// safe to call for every message.
bool ota_mqtt_consume(const char* topic, const uint8_t* payload, unsigned int len);

// Run one full chunked OTA on the MQTT task (call from the RPC drain, AFTER the
// RPC has been acked): meta handshake -> Update.begin -> pull+write+hash each
// chunk -> verify sha256 -> Update.end -> mark success -> reboot. Reboots on
// success (no return); on failure it publishes state:FAILED and returns.
//   is_fs=false — app firmware ("ota_mqtt" RPC):  "ota/" subtree, U_FLASH.
//   is_fs=true  — filesystem  ("ota_mqtt_fs"):    "otafs/" subtree, U_SPIFFS.
// The fs path additionally: checks the optional meta "target" echo, checks the
// image fits the fs partition, and unmounts LittleFS before flashing (so the
// only exit from a started fs flash is reboot — success OR failure).
OtaResult ota_mqtt_run(bool is_fs);

// Resume a PAUSED OTA (stalled mid-download). Call from com_loop after the link
// is back up, gated by ota_mqtt_pending(): re-subscribes, re-sends meta/req, and
// continues chunk pull from the saved offset. Reboots on completion.
OtaResult ota_mqtt_resume();

// True while an OTA is paused waiting for the MQTT link to recover.
bool ota_mqtt_pending();

// True while an OTA session exists (running or paused). The ota_mqtt RPC handler
// rejects a duplicate trigger when this is set, so a re-sent RPC can't destroy a
// paused session that is waiting to resume.
bool ota_mqtt_active();

// ── Success / abort across the reboot ────────────────────────────────────────
// A good flash reboots, so the outcome is confirmed on the NEXT boot: markers
// are stored in NVS before restart; ota_boot_check() (in setup) latches them,
// and the transport calls the take_* functions after it reconnects to publish
// the outcome on the right subtree. *was_fs tells the caller WHICH image the
// boot event belongs to: app -> ota/state UPDATED {current_fw_version} or
// FAILED "aborted"; fs -> otafs/state UPDATED {fs_version} or FAILED "aborted".
void ota_mark_boot_success();   // call right before ESP.restart() on OTA_OK
void ota_boot_check();          // call once in setup(); latches the NVS markers

// True once if this boot followed a successful OTA. For an fs flash, fsver_out
// (if non-null) receives the meta version label persisted before the reboot.
bool ota_boot_take_success(bool* was_fs, char* fsver_out, size_t len);

// True once if this boot followed an INTERRUPTED OTA (reset / power-loss /
// crash mid-download) — publish state:FAILED err:"aborted" on the *was_fs tree
// so the server-side state doesn't stay stuck at DOWNLOADING.
bool ota_boot_take_aborted(bool* was_fs);

#endif // OTA_H
