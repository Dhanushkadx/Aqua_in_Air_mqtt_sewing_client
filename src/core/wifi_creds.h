#ifndef WIFI_CREDS_H
#define WIFI_CREDS_H

#include <Arduino.h>

// ── WiFi credentials store ────────────────────────────────────────────────────
//
// Persists WiFi SSID/password and a one-shot "pending_portal" flag in
// /wifi.json on LittleFS. Falls back to compile-time defaults from wifi_com.cpp
// if the file is missing or empty so existing field devices keep working
// across an OTA that introduces this module for the first time.
//
// Flow:
//   - On boot: wifi_creds_init() loads /wifi.json (or defaults).
//   - initWiFi_STA() calls wifi_creds_get_ssid()/_get_pass() to connect.
//   - User triggers portal via button OR RPC "enter_portal".
//   - RPC path: wifi_creds_set_pending_portal(true) -> save -> reboot.
//   - Portal mode: wifi_creds_save_wifi(ssid, pass) -> reboot.
//   - On the post-save boot, portal_should_enter() returns false and the
//     device connects to the new network normally.
//
// All functions are safe to call after config_init() has mounted LittleFS.

// Load /wifi.json into module state. Called once early in setup().
// If the file is missing, populates state from the compile-time defaults
// (ssid/password globals in wifi_com.cpp) so behavior is identical to before.
void wifi_creds_init();

// Currently-active SSID and password — used by initWiFi_STA().
const char* wifi_creds_get_ssid();
const char* wifi_creds_get_pass();

// Persist new credentials from the portal save handler. Returns true on
// successful flash write. Caller is expected to reboot after this.
bool wifi_creds_save_wifi(const char* ssid, const char* pass);

// One-shot flag — set by the enter_portal RPC, checked by portal_should_enter()
// on the next boot, and cleared by the portal as soon as it starts. This
// ensures a reboot during normal operation only enters the portal once.
bool wifi_creds_get_pending_portal();
bool wifi_creds_set_pending_portal(bool pending);

#endif // WIFI_CREDS_H
