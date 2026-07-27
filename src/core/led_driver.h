#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <Arduino.h>

// ── Per-pixel LED channel driver ─────────────────────────────────────────────
//
// Low-level driver for the WS2812/NeoPixel chain (NUM_LEDS pixels on RGB_LED_PIN,
// pinsx.h). Each pixel is an INDEPENDENT channel: its own colour, mode and blink
// timing. Setters are thread-safe (a static spinlock, no heap) so any task/core
// can update a channel; a single led_tick() in loop() renders all pixels.
//
// Deliberately NO dedicated task: a task would need a heap-allocated stack, which
// fragments the heap and eats into the large contiguous block the OTA TLS
// handshake needs. Rendering from loop() also keeps the LEDs updating DURING an
// OTA (the download blocks Task1, but loop() runs independently). RAM cost is a
// small static channel array + one spinlock — zero heap.
//
// This driver knows nothing about device state — see led_status.* for the policy
// that maps subsystems (WiFi, cloud, pump…) to pixels.

enum LedMode : uint8_t {
    LED_MODE_OFF = 0,   // dark
    LED_MODE_STEADY,    // solid colour
    LED_MODE_BLINK,     // on_ms on, off_ms off, repeating
    LED_MODE_PULSE      // smooth breathe; on_ms = full period (off_ms ignored)
};

// Init the strip. Call once in setup(). No-op if HAS_PIXEL is undefined.
void led_driver_begin();

// Set one pixel's channel (thread-safe). color = 0xRRGGBB. Calling with the SAME
// parameters is idempotent — it does NOT restart the blink phase — so a status
// poller can call this every tick without the LED ever stuttering.
void led_set(uint8_t idx, uint32_t color, LedMode mode, uint16_t on_ms, uint16_t off_ms);

// Convenience: turn a pixel off.
void led_off(uint8_t idx);

// Render all pixels. Call often from loop(); internally rate-limited (~50 fps)
// so it never bit-bangs the strip (interrupts-off window) more than needed.
void led_tick();

#endif // LED_DRIVER_H
