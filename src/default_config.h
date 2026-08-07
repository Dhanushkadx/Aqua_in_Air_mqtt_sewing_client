/*
 * default_config.h
 *
 * ONE place for every compile-time default in the firmware. Edit here, not in
 * the scattered .cpp files — they all read from these macros.
 *
 * Split (see the review decisions):
 *   - Per-PCB-board values live in the #ifdef block at the bottom, keyed off the
 *     board macro selected in pinsx.h (IOT_PULSE_X / PLC_IOT_BRIDGE / VERO_BOARD).
 *     Only the board MODEL string differs per board here; pin maps stay in pinsx.h.
 *   - Everything else is global (one copy for all AquaSew devices).
 *
 * NOT here, on purpose:
 *   - FW_VER — a build flag in platformio.ini (OTA success-reporting reads it).
 *   - The MQTT CA certificate — a pinned cert, not an operator-tunable value;
 *     it stays inline in core/wifi_mqtt.cpp.
 */

#ifndef _DEFAULT_CONFIG_H
#define _DEFAULT_CONFIG_H

#include "pinsx.h"   // board-selection macro (IOT_PULSE_X, ...) for the model block

// ── MQTT transport (all AquaSew devices share this broker) ────────────────────
#define DEFAULT_TOPIC_ROOT   "aquasew"                                  // topic prefix
#define DEFAULT_MQTT_HOST    "j0117d13.ala.asia-southeast1.emqxsl.com"  // EMQX Cloud, TLS
#define DEFAULT_MQTT_PORT    8883
#define DEFAULT_MQTT_USER    "Aqua"
#define DEFAULT_MQTT_PASS    "aqua123"

// ── WiFi STA seed credentials — THE single source of truth ────────────────────
// Seeds /wifi.json on first boot (wifi_creds.cpp), which is the store the STA
// link actually reads (initWiFi_STA -> wifi_creds_get_ssid). Also seeds the
// system_config wifissid_sta/wifipass_sta fields (ConfigManager) so there is one
// place to edit. After first boot the portal-saved /wifi.json wins at runtime.
#define DEFAULT_WIFI_SSID    "MAS-IoT"
#define DEFAULT_WIFI_PASS    "welcome@2026"

// ── WiFi enrollment portal (AP mode) ──────────────────────────────────────────
#define DEFAULT_PORTAL_AP_PREFIX  "PulseX-Setup-"   // SSID becomes <prefix><MAC4>
#define DEFAULT_PORTAL_AP_PASS    "pulsex2026"       // WPA2 requires >= 8 chars

// ── Telemetry pacing ──────────────────────────────────────────────────────────
#define DEFAULT_TELE_INTERVAL_S       60    // fallback when updates_interval unset
#define DEFAULT_HEARTBEAT_INTERVAL_S  300   // event_type "heartbeat" cadence

// ── system_config.json seed values (writeDefaultSystemConfig) ─────────────────
// These are RUNTIME-editable via the web UI; the values below are just what a
// factory-fresh config file is seeded with.
#define DEFAULT_HTTP_USER          "admin"
#define DEFAULT_HTTP_PASS          "admin"
// The STA seed comes from DEFAULT_WIFI_SSID/PASS above (single source of truth) —
// ConfigManager seeds wifissid_sta/wifipass_sta from those. Only the config-mode
// AP fields, which are a distinct concept, live here.
#define DEFAULT_CFG_AP_SSID        ""
#define DEFAULT_CFG_AP_PASS        ""
#define DEFAULT_UPDATES_INTERVAL_S 30              // tele publish interval
#define DEFAULT_WIFI_RECONNECT_S   30
#define DEFAULT_REALTIME           false
#define DEFAULT_SERVER_PORT        "1883"
#define DEFAULT_DEVICE_LOCATION    "module_"
#define DEFAULT_MACHINE_TYPE       "single needle"
#define DEFAULT_OPERATION_NAME     "any operation"
#define DEFAULT_MACHINE_SERIAL     "000000000000"
#define DEFAULT_PRESCALE           1               // production pulses per count

// ── Per-PCB-board values ──────────────────────────────────────────────────────
// Board model string reported in hw_caps (attr/pub). Keyed off the board macro
// chosen in pinsx.h. Pin maps themselves stay in pinsx.h.
#if defined(IOT_PULSE_X)
  #define DEFAULT_BOARD_MODEL  "IOT_PULSE_X"
#elif defined(PLC_IOT_BRIDGE)
  #define DEFAULT_BOARD_MODEL  "PLC_IOT_BRIDGE"
#elif defined(VERO_BOARD)
  #define DEFAULT_BOARD_MODEL  "VERO_BOARD"
#else
  #define DEFAULT_BOARD_MODEL  "UNKNOWN"
#endif

#endif // _DEFAULT_CONFIG_H
