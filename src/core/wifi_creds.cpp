#include "wifi_creds.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define WIFI_CREDS_FILE "/wifi.json"

// Compile-time defaults — declared in wifi_com.cpp. Used as the seed values
// when /wifi.json does not yet exist, so an OTA that introduces the portal
// to an already-deployed device behaves identically to the previous build.
extern const char* ssid;
extern const char* password;

// In-RAM mirror of the persisted file. wifi_creds_init() fills these, then
// all reads serve from RAM (no LittleFS round-trip per call).
static char s_ssid[64]      = {};
static char s_pass[64]      = {};
static bool s_pending_portal = false;

// Serialize current state to /wifi.json. Caller must already know LittleFS
// is mounted (config_init() does that during setup()).
static bool save_to_flash() {
    StaticJsonDocument<256> doc;
    doc["ssid"]           = s_ssid;
    doc["pass"]           = s_pass;
    doc["pending_portal"] = s_pending_portal;

    File f = LittleFS.open(WIFI_CREDS_FILE, "w");
    if (!f) {
        Serial.println(F("wifi_creds: cannot open /wifi.json for write"));
        return false;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}

void wifi_creds_init() {
    // Seed RAM state with compile-time defaults first so anything we fail to
    // load from disk still has a sane value to fall back on.
    strlcpy(s_ssid, ssid     ? ssid     : "", sizeof(s_ssid));
    strlcpy(s_pass, password ? password : "", sizeof(s_pass));
    s_pending_portal = false;

    File f = LittleFS.open(WIFI_CREDS_FILE, "r");
    if (!f) {
        // First boot after OTA — write the defaults out so subsequent boots
        // read from disk and the portal save path has a real file to update.
        Serial.println(F("wifi_creds: /wifi.json absent — seeding with compile-time defaults"));
        save_to_flash();
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("wifi_creds: parse error '%s' — keeping defaults\n", err.c_str());
        return;
    }

    // Only overwrite a field if it actually exists in the JSON, so partial
    // files (e.g. saved without pending_portal) don't erase known-good values.
    if (doc.containsKey("ssid")) strlcpy(s_ssid, doc["ssid"] | "", sizeof(s_ssid));
    if (doc.containsKey("pass")) strlcpy(s_pass, doc["pass"] | "", sizeof(s_pass));
    s_pending_portal = doc["pending_portal"] | false;

    Serial.printf("wifi_creds: loaded ssid='%s' pending_portal=%d\n",
        s_ssid, s_pending_portal ? 1 : 0);
}

const char* wifi_creds_get_ssid() { return s_ssid; }
const char* wifi_creds_get_pass() { return s_pass; }

bool wifi_creds_save_wifi(const char* new_ssid, const char* new_pass) {
    if (!new_ssid) return false;
    strlcpy(s_ssid, new_ssid,            sizeof(s_ssid));
    strlcpy(s_pass, new_pass ? new_pass : "", sizeof(s_pass));
    // Clear the one-shot flag as part of the save — the next boot connects
    // with the new credentials, not back into the portal.
    s_pending_portal = false;
    bool ok = save_to_flash();
    if (ok) Serial.printf("wifi_creds: saved ssid='%s'\n", s_ssid);
    return ok;
}

bool wifi_creds_get_pending_portal() { return s_pending_portal; }

bool wifi_creds_set_pending_portal(bool pending) {
    s_pending_portal = pending;
    return save_to_flash();
}
