#ifndef TIMERCONTROL_H
#define TIMERCONTROL_H

#include <Arduino.h>
#include <RTClib.h>   // DateTime — always needed regardless of HAS_RTC
#include "../pinsx.h"

// ── Time subsystem — priority chain ───────────────────────────────────────────
//
//  The live timekeeping source is ALWAYS the ESP32 internal RTC
//  (settimeofday / time()). The three sync sources below write into it:
//
//  Priority 1 — DS3231 hardware RTC  (requires HAS_RTC + module on I2C)
//    On boot: reads DS3231 → seeds ESP32 internal clock immediately.
//    Advantage: time survives power cycles via battery-backed crystal.
//
//  Priority 2 — GSM NITZ
//    Carrier broadcasts time on network registration (no data session needed).
//    Sets ESP32 internal clock AND DS3231 (if present).
//
//  Priority 3 — WiFi NTP
//    Fetched once after WiFi connects.
//    Sets ESP32 internal clock AND DS3231 (if present).
//
//  Fallback: if no source has set the clock yet, rtc_ready() returns false
//    and the schedule engine holds off — no phantom firings with epoch 0.
//
// ─────────────────────────────────────────────────────────────────────────────

// Call once in setup() after I2C is up (extender_init() starts Wire).
// Attempts DS3231 first. If a valid time is found, seeds the ESP32 internal
// clock immediately so schedules work before the network connects.
void timercontrol_init();

// Returns true once any source (DS3231, NITZ, or NTP) has set the clock.
// The schedule engine will not fire any entries until this returns true.
bool rtc_ready();

// Returns the current time from the ESP32 internal clock.
// Always check rtc_ready() first — DateTime is meaningless until clock is set.
DateTime rtc_now();

// GSM NITZ path — call after modem.getNetworkTime() succeeds.
//
// NITZ delivers the LOCAL date/time at the cell tower plus a tz offset (hours,
// can be fractional like +5.5 for SL or +5.75 for Nepal). To keep the ESP32
// internal clock in true UTC — same convention the WiFi NTP path uses — we
// subtract tz_hours before storing. Pass the tz value returned by
// modem.getNetworkTime() verbatim.
//
// Sets ESP32 internal clock. Also updates DS3231 if present.
// No-op after the first successful sync this boot.
void ntp_sync_rtc_gsm(int year, int month, int day, int hour, int minute, int second, float tz_hours);

// WiFi NTP path — call periodically from Task3 while WiFi is connected.
// Sets ESP32 internal clock. Also updates DS3231 if present.
// No-op after the first successful sync this boot.
void ntp_sync_rtc();

// True after the first successful network sync (NITZ or NTP) this boot.
// Prevents redundant syncs on reconnect.
bool ntp_synced();

#endif // TIMERCONTROL_H
