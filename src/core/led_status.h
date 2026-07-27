#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>

// ── Status-indicator policy (7-pixel chain) ──────────────────────────────────
// Maps live device state to the LED chain and renders it. This is the ONLY place
// that decides "what colour/blink means what". Fixed per-pixel meaning:
//   [0] heartbeat / power      [1] WiFi          [2] GSM / cellular
//   [3] cloud (MQTT)           [4] pump          [5] valve activity
//   [6] schedule / OTA
//
// Poll-based: led_status_tick() re-reads state on a slow cadence and drives each
// pixel via the thread-safe led_driver setters; the driver renders at ~50 fps.
// No scattered pixel calls elsewhere — subsystems just expose their state.

// ── GSM link state (GSM build only) ─────────────────────────────────────────
// The GSM transport publishes its modem progress here so pixel [2] can show a
// three-way status (no SIM / searching / registered) without the LED poll ever
// touching the modem or MQTT client from loopTask. Declared here (the indicator
// contract) but only defined + written in gprs_mqtt.cpp, and only read under
// #ifndef WLAN — the WiFi build never references it.
enum GsmLinkState : uint8_t {
    GSM_DOWN       = 0,   // idle / dropped — nothing up yet
    GSM_NO_SIM     = 1,   // modem alive but SIM not READY  → yellow blink
    GSM_SEARCHING  = 2,   // waiting for network registration → red blink
    GSM_REGISTERED = 3,   // registered on the carrier network → green steady
};
extern volatile uint8_t g_gsm_state;

// Init the driver and set the boot pattern. Call once in setup().
void led_status_begin();

// Call frequently from loop(): re-evaluates state (throttled ~3 Hz) and renders.
void led_status_tick();

// OTA-in-progress flag → drives pixel [6] (yellow blink, beats the schedule
// indicator). Set true before an OTA download; set false on failure. (Success
// reboots, so no reset is needed.)
void led_status_set_ota(bool active);

#endif // LED_STATUS_H
