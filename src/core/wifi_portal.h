#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>

// ── WiFi enrollment portal (AP + captive DNS + web UI) ────────────────────────
//
// Drives a self-contained "give me your WiFi credentials" experience over a
// SoftAP. Started either by holding PROGRAM_PIN low at boot for ~3 seconds, or
// by setting the pending_portal flag via the enter_portal RPC and rebooting.
//
// Boot sequence (in main.cpp):
//   wifi_creds_init();
//   if (portal_should_enter()) portal_run();   // never returns
//   ... normal task creation, GSM/MQTT, schedule executor ...
//
// While the portal is running, NOTHING else runs — GSM, MQTT, schedule executor
// and telemetry are all suspended. A 10-minute idle watchdog reboots the device
// back into normal mode so an abandoned portal cannot strand the unit forever.
//
// AP details:
//   SSID:     "PrimeHive-Setup-<MAC4>"
//   Password: "primehive2026"  (WPA2 requires >= 8 chars)
//   IP:       192.168.4.1
//   DNS:      captive — every host resolves to the AP IP

// True if either condition is met:
//   - PROGRAM_PIN held LOW continuously for ~3 seconds at this call
//   - /wifi.json has pending_portal=true (set by the enter_portal RPC)
// Safe to call after config_init() and wifi_creds_init() have run.
bool portal_should_enter();

// Start the portal — NEVER returns. Internally drives WiFi.softAP, DNSServer,
// and AsyncWebServer until either credentials are saved (-> reboot) or the
// 10-minute idle timeout expires (-> reboot).
void portal_run();

#endif // WIFI_PORTAL_H
