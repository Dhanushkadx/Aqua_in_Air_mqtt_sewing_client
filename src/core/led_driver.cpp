#include "led_driver.h"
#include "../pinsx.h"

#ifdef HAS_PIXEL
#include <Adafruit_NeoPixel.h>
#include "freertos/FreeRTOS.h"
#include <string.h>

// One channel's intended state. Rendered independently every frame from `t0`.
struct LedCh {
    uint32_t color;    // 0xRRGGBB
    uint16_t on_ms;    // BLINK on time / PULSE full period
    uint16_t off_ms;   // BLINK off time
    uint8_t  mode;     // LedMode
    uint32_t t0;       // phase anchor (millis at last change)
};

static LedCh          s_ch[NUM_LEDS];
static portMUX_TYPE   s_mux = portMUX_INITIALIZER_UNLOCKED;   // static → no heap
static Adafruit_NeoPixel s_strip(NUM_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

static const uint8_t  LED_BRIGHTNESS = 60;    // dim — these are indicators, ~24%
static const uint16_t LED_FRAME_MS   = 20;    // render cap ≈ 50 fps

void led_driver_begin() {
    for (uint8_t i = 0; i < NUM_LEDS; i++) s_ch[i] = { 0, 0, 0, LED_MODE_OFF, 0 };
    s_strip.begin();
    s_strip.setBrightness(LED_BRIGHTNESS);
    s_strip.clear();
    s_strip.show();
}

void led_set(uint8_t idx, uint32_t color, LedMode mode, uint16_t on_ms, uint16_t off_ms) {
    if (idx >= NUM_LEDS) return;
    portENTER_CRITICAL(&s_mux);
    LedCh& c = s_ch[idx];
    // Idempotent: only reset the phase anchor if something actually changed, so a
    // per-tick poller re-asserting the same state doesn't restart the blink.
    if (c.color != color || c.mode != (uint8_t)mode || c.on_ms != on_ms || c.off_ms != off_ms) {
        c.color = color; c.mode = (uint8_t)mode; c.on_ms = on_ms; c.off_ms = off_ms;
        c.t0 = millis();
    }
    portEXIT_CRITICAL(&s_mux);
}

void led_off(uint8_t idx) { led_set(idx, 0, LED_MODE_OFF, 0, 0); }

// Scale a 0xRRGGBB colour by 0..255 (for the pulse breathe envelope).
static uint32_t scale_color(uint32_t c, uint8_t b) {
    uint16_t r = ((c >> 16) & 0xFF) * b / 255;
    uint16_t g = ((c >> 8)  & 0xFF) * b / 255;
    uint16_t bl = (c        & 0xFF) * b / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

void led_tick() {
    static uint32_t s_last = 0;
    uint32_t now = millis();
    if ((uint32_t)(now - s_last) < LED_FRAME_MS) return;   // rate-limit the bit-bang
    s_last = now;

    // Snapshot the channel state under the lock, then render outside it (never
    // hold the spinlock across the ~200 µs strip.show()).
    LedCh snap[NUM_LEDS];
    portENTER_CRITICAL(&s_mux);
    memcpy(snap, s_ch, sizeof(snap));
    portEXIT_CRITICAL(&s_mux);

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        uint32_t col = 0;
        switch (snap[i].mode) {
            case LED_MODE_STEADY:
                col = snap[i].color;
                break;
            case LED_MODE_BLINK: {
                uint16_t period = snap[i].on_ms + snap[i].off_ms;
                if (period == 0) period = 1;
                uint32_t ph = (now - snap[i].t0) % period;
                col = (ph < snap[i].on_ms) ? snap[i].color : 0;
                break;
            }
            case LED_MODE_PULSE: {
                uint16_t period = snap[i].on_ms ? snap[i].on_ms : 1000;
                uint32_t ph = (now - snap[i].t0) % period;
                // Triangle envelope 0→255→0 across the period.
                uint8_t b = (ph < (uint32_t)period / 2)
                          ? (uint8_t)(ph * 2 * 255 / period)
                          : (uint8_t)(255 - (ph - period / 2) * 2 * 255 / period);
                col = scale_color(snap[i].color, b);
                break;
            }
            case LED_MODE_OFF:
            default:
                col = 0;
                break;
        }
        s_strip.setPixelColor(i, col);
    }
    s_strip.show();
}

#else  // !HAS_PIXEL — no-op stubs so callers need no guards

void led_driver_begin() {}
void led_set(uint8_t, uint32_t, LedMode, uint16_t, uint16_t) {}
void led_off(uint8_t) {}
void led_tick() {}

#endif // HAS_PIXEL
