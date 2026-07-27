#include "led_status.h"
#include "led_driver.h"
#include "../pinsx.h"

#ifdef HAS_PIXEL
#include <WiFi.h>

// IMPORTANT: this poll runs on loopTask, so it must only read thread-safe cached
// flags — never the MQTT client (PubSubClient/WiFiClientSecure are not
// thread-safe and touching them off the MQTT task corrupts the TLS session).
// The transport maintains g_mqtt_online for exactly this reason.
extern volatile bool g_mqtt_online;   // defined in wifi_mqtt.cpp

// ── Colours (0xRRGGBB) ───────────────────────────────────────────────────────
#define C_GREEN    0x00FF00
#define C_RED      0xFF0000
#define C_WHITE    0xFFFFFF   // OTA in progress
#define C_PURPLE   0x6000FF   // blue + a little red — "cloud connected" breathe

static volatile bool s_ota_active = false;
static bool          s_ota_frozen = false;   // pixel rendered once for OTA, then paused

void led_status_set_ota(bool active) { s_ota_active = active; }

// ═══ Single onboard-LED policy ════════════════════════════════════════════════
// This board carries the whole CONNECTIVITY state machine on one pixel (+ OTA
// via the freeze path in led_status_tick). Convention: BLINK = working on it;
// PURPLE BREATHE = fully connected (a breathe, never steady, so it doesn't look
// like a stuck LED).
static void poll_and_apply() {
    if (g_mqtt_online) {                                   // cloud connected — done
        led_set(0, C_PURPLE, LED_MODE_PULSE, 2200, 0);     // purple breathe
        return;
    }
    // connecting-to-WiFi (red) -> WiFi up, reaching cloud (green)
    if (WiFi.status() == WL_CONNECTED) led_set(0, C_GREEN, LED_MODE_BLINK, 300, 300);
    else                               led_set(0, C_RED,   LED_MODE_BLINK, 300, 300);
}

void led_status_begin() {
    led_driver_begin();
    led_set(0, C_RED, LED_MODE_BLINK, 300, 300);           // "connecting" from boot
}

void led_status_tick() {
    // During an OTA the firmware streams bytes over the live link. NeoPixel
    // show() disables interrupts to bit-bang the WS2812 timing, and doing that
    // repeatedly can drop link RX bytes → transfer stalls. So freeze the pixel
    // for the whole OTA window: render the OTA indicator exactly once (steady,
    // not blinking → no repeat show()), then leave the link alone until OTA ends
    // (reboot on success, or set_ota(false) resumes rendering on failure).
    if (s_ota_active) {
        if (!s_ota_frozen) {
            led_set(0, C_WHITE, LED_MODE_STEADY, 0, 0);   // steady white during OTA
            led_tick();               // one render to light it, then silence
            s_ota_frozen = true;
        }
        return;
    }
    s_ota_frozen = false;

    static uint32_t s_poll = 0;
    uint32_t now = millis();
    if ((uint32_t)(now - s_poll) >= 300) {   // re-read state ~3 Hz
        s_poll = now;
        poll_and_apply();
    }
    led_tick();   // render (rate-limited to ~50 fps inside)
}

#else  // !HAS_PIXEL — board has no LED at all; every entry point is a no-op

void led_status_begin() {}
void led_status_tick() {}
void led_status_set_ota(bool) {}

#endif // HAS_PIXEL
