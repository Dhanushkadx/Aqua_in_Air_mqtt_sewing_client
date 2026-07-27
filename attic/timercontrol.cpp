#include "timercontrol.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>        // settimeofday(), time()

// ── Module state ───────────────────────────────────────────────────────────────
static bool s_clock_ready = false;  // true once any source has successfully set the clock
static bool s_ntp_synced  = false;  // true after first network sync (NITZ or NTP) this boot

// NTP client — used once after WiFi connects, then released
static WiFiUDP   s_ntp_udp;
static NTPClient s_ntp_client(s_ntp_udp, "pool.ntp.org", 0); // UTC, no auto-update

#ifdef HAS_RTC
// DS3231 driver — private to this file, accessed externally only via rtc_now()
static RTC_DS3231 s_ds3231;
static bool       s_ds3231_ok = false; // true if DS3231 was found and responded on I2C
#endif

// ── Internal helper ────────────────────────────────────────────────────────────

// Single point for writing a Unix epoch into the ESP32 internal RTC.
// All three sync paths (DS3231, NITZ, NTP) funnel through here.
static void apply_epoch(uint32_t epoch) {
    struct timeval tv = { .tv_sec = (time_t)epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    s_clock_ready = true;
}

// ── Public API ─────────────────────────────────────────────────────────────────

void timercontrol_init() {

#ifdef HAS_RTC
    // ── Priority 1: DS3231 hardware RTC ──────────────────────────────────────
    // Wire is already started by extender_init() — just probe the chip.
    if (s_ds3231.begin()) {
        DateTime t = s_ds3231.now();

        if (t.year() >= 2024) {
            // DS3231 holds a plausible time — seed the ESP32 internal clock right
            // away so schedules fire immediately without waiting for the network.
            apply_epoch(t.unixtime());
            s_ds3231_ok = true;
            Serial.printf("DS3231 ready — clock seeded: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
            return; // highest-priority source succeeded — no need to wait for network
        }

        // DS3231 found but its time is invalid (battery dead or first use).
        // Keep s_ds3231_ok = true so NITZ/NTP can persist into it later.
        s_ds3231_ok = true;
        Serial.println(F("DS3231 found but time invalid (battery dead?) — waiting for network sync"));

    } else {
        Serial.println(F("DS3231 not found on I2C — waiting for network sync"));
    }
#else
    Serial.println(F("RTC module not compiled in — waiting for network sync"));
#endif

    // No valid time source yet — rtc_ready() returns false until network syncs
}

bool rtc_ready() {
    return s_clock_ready;
}

DateTime rtc_now() {
    // Always read from the ESP32 internal clock — it is the live source and is
    // kept in sync by whichever source last called apply_epoch().
    return DateTime((uint32_t)time(nullptr));
}

void ntp_sync_rtc_gsm(int year, int month, int day, int hour, int minute, int second, float tz_hours) {
    if (s_ntp_synced) return; // already synced this boot — DS3231 already up to date

    // Sanity check — SIM800 sometimes returns garbage before network registration
    if (year < 2024 || year > 2099) {
        Serial.printf("NITZ: invalid year %d — ignoring\n", year);
        return;
    }
    // NITZ tz offsets in the wild are within roughly +/- 14 hours.
    // Anything beyond that is a parse / garbage value — drop it.
    if (tz_hours < -14.0f || tz_hours > 14.0f) {
        Serial.printf("NITZ: implausible tz offset %.2f — ignoring\n", tz_hours);
        return;
    }

    // The fields delivered by NITZ are LOCAL at the cell tower. Subtract the
    // tz offset to convert to true UTC before storing — keeps the ESP32 clock
    // in the same convention used by WiFi NTP, so the schedule engine never
    // needs to know which transport synced the clock.
    DateTime local(year, month, day, hour, minute, second);
    uint32_t utc_epoch = (uint32_t)((int64_t)local.unixtime() - (int32_t)(tz_hours * 3600.0f));

    // Step 1: always set the ESP32 internal clock — this is the live time source
    apply_epoch(utc_epoch);
    s_ntp_synced = true;

    // Render the stored UTC for logs (vs the local NITZ values for context)
    DateTime utc((uint32_t)utc_epoch);

#ifdef HAS_RTC
    // Step 2: persist UTC into DS3231 so time survives the next power cycle
    if (s_ds3231_ok) {
        s_ds3231.adjust(utc);
        Serial.printf("NITZ sync → ESP32 + DS3231: local %04d-%02d-%02d %02d:%02d:%02d tz%+.2f → %04d-%02d-%02d %02d:%02d:%02d UTC\n",
            year, month, day, hour, minute, second, tz_hours,
            utc.year(), utc.month(), utc.day(), utc.hour(), utc.minute(), utc.second());
    } else {
        Serial.printf("NITZ sync → ESP32 only (no DS3231): local %04d-%02d-%02d %02d:%02d:%02d tz%+.2f → %04d-%02d-%02d %02d:%02d:%02d UTC\n",
            year, month, day, hour, minute, second, tz_hours,
            utc.year(), utc.month(), utc.day(), utc.hour(), utc.minute(), utc.second());
    }
#else
    Serial.printf("NITZ sync → ESP32: local %04d-%02d-%02d %02d:%02d:%02d tz%+.2f → %04d-%02d-%02d %02d:%02d:%02d UTC\n",
        year, month, day, hour, minute, second, tz_hours,
        utc.year(), utc.month(), utc.day(), utc.hour(), utc.minute(), utc.second());
#endif
}

void ntp_sync_rtc() {
    if (s_ntp_synced)                  return; // already done this boot
    if (WiFi.status() != WL_CONNECTED) return; // not connected yet — try again next call

    s_ntp_client.begin();

    // forceUpdate() blocks until the server replies (~<500 ms) or times out
    if (!s_ntp_client.forceUpdate()) {
        Serial.println(F("NTP: request timed out — will retry"));
        s_ntp_client.end();
        return;
    }

    uint32_t epoch = (uint32_t)s_ntp_client.getEpochTime();
    s_ntp_client.end(); // release UDP socket — no longer needed

    // Step 1: set the ESP32 internal clock
    apply_epoch(epoch);
    s_ntp_synced = true;

#ifdef HAS_RTC
    // Step 2: persist into DS3231 so time survives the next power cycle
    if (s_ds3231_ok) {
        s_ds3231.adjust(DateTime(epoch));
        Serial.println(F("NTP sync → ESP32 + DS3231 updated"));
    } else {
        Serial.println(F("NTP sync → ESP32 updated (no DS3231)"));
    }
#else
    Serial.println(F("NTP sync → ESP32 updated"));
#endif
}

bool ntp_synced() {
    return s_ntp_synced;
}
