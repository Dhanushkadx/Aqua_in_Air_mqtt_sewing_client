#include "wifi_portal.h"
#include "wifi_creds.h"
#include "../pinsx.h"
#include "../default_config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ── Tunables ──────────────────────────────────────────────────────────────────
#define PORTAL_AP_PASSWORD     DEFAULT_PORTAL_AP_PASS   // WPA2 requires >= 8 chars
#define PORTAL_BUTTON_HOLD_MS  3000               // hold time to enter portal at boot
#define PORTAL_IDLE_TIMEOUT_MS (10UL * 60 * 1000) // 10 min then reboot to normal

// ── Single server instance, owns DNS + HTTP for the portal lifetime ───────────
static AsyncWebServer server(80);
static DNSServer      dnsServer;

// Touched whenever the portal serves any HTTP request — resets the idle
// watchdog so a user actively interacting with the page is not kicked out.
static volatile uint32_t s_last_activity_ms = 0;

// ── Embedded portal page ──────────────────────────────────────────────────────
// Single file, PROGMEM to keep it out of RAM. Plain HTML + a small JS that:
//   1. calls /api/scan and renders detected networks as a clickable list
//   2. fills the SSID field when one is picked
//   3. POSTs the form as JSON to /api/save, then shows "Saved — rebooting".
// Kept deliberately minimal (no CSS framework, no images) so the binary
// growth stays around 3 KB.
static const char PORTAL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PulseX WiFi Setup</title>
<style>
body{font-family:system-ui,sans-serif;max-width:420px;margin:24px auto;padding:0 16px;color:#222}
h1{font-size:1.3em;margin:0 0 4px} .sub{color:#666;font-size:.9em;margin-bottom:18px}
label{display:block;margin:12px 0 4px;font-weight:600}
input{width:100%;padding:8px;border:1px solid #bbb;border-radius:6px;font-size:1em;box-sizing:border-box}
button{width:100%;padding:10px;margin-top:16px;border:0;border-radius:6px;background:#0a6;color:#fff;font-size:1em;cursor:pointer}
button:disabled{background:#888}
.nets{margin-top:8px;border:1px solid #ddd;border-radius:6px;max-height:200px;overflow:auto}
.net{padding:8px 12px;cursor:pointer;border-bottom:1px solid #eee;display:flex;justify-content:space-between}
.net:last-child{border-bottom:0} .net:hover{background:#f5f5f5}
.rssi{color:#888;font-size:.85em}
.status{margin-top:14px;padding:10px;border-radius:6px;display:none}
.status.ok{background:#dfd;color:#063} .status.err{background:#fdd;color:#600}
</style></head><body>
<h1>PulseX WiFi Setup</h1>
<div class="sub">Pick a network or type it in. Device will reboot after save.</div>

<label>Available networks <button id="rescan" style="float:right;width:auto;padding:2px 8px;margin:0;font-size:.85em">Scan</button></label>
<div id="nets" class="nets" style="padding:10px;color:#888">Press Scan to list networks.</div>

<label for="ssid">SSID</label>
<input id="ssid" placeholder="Wi-Fi name" autocomplete="off">
<label for="pass">Password</label>
<input id="pass" type="password" placeholder="leave blank for open network" autocomplete="off">

<button id="save">Save and reboot</button>
<div id="status" class="status"></div>

<script>
const $=q=>document.querySelector(q);
function showStatus(msg,ok){const s=$("#status");s.textContent=msg;s.className="status "+(ok?"ok":"err");s.style.display="block";}
async function scan(){
  $("#nets").textContent="Scanning…"; $("#rescan").disabled=true;
  // Poll up to 15 s — first /api/scan call kicks off the scan, results land
  // ~3 s later. We retry every second while the device reports scanning:true.
  for(let i=0;i<15;i++){
    try{
      const r=await fetch("/api/scan"); const j=await r.json();
      if(j.scanning){await new Promise(s=>setTimeout(s,1000));continue;}
      const list=j.nets||[];
      if(!list.length){$("#nets").textContent="No networks found.";break;}
      $("#nets").innerHTML="";
      list.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
        const d=document.createElement("div"); d.className="net";
        d.innerHTML='<span>'+(n.secure?"&#128274; ":"")+n.ssid+'</span><span class="rssi">'+n.rssi+' dBm</span>';
        d.onclick=()=>{$("#ssid").value=n.ssid;$("#pass").focus();};
        $("#nets").appendChild(d);
      });
      break;
    }catch(e){$("#nets").textContent="Scan failed.";break;}
  }
  $("#rescan").disabled=false;
}
$("#rescan").onclick=scan;
$("#save").onclick=async()=>{
  const ssid=$("#ssid").value.trim(); if(!ssid){showStatus("SSID required",false);return;}
  $("#save").disabled=true; showStatus("Saving…",true);
  try{
    const r=await fetch("/api/save",{method:"POST",headers:{"Content-Type":"application/json"},
      body:JSON.stringify({ssid:ssid,pass:$("#pass").value})});
    const j=await r.json();
    if(j.ok){showStatus("Saved. Rebooting now — reconnect to your normal WiFi.",true);}
    else{showStatus("Save failed: "+(j.error||"unknown"),false); $("#save").disabled=false;}
  }catch(e){showStatus("Network error",false); $("#save").disabled=false;}
};
</script></body></html>)HTML";

// ── Helpers ───────────────────────────────────────────────────────────────────

// Build the current SSID scan as a JSON array — NON-BLOCKING.
//
// AsyncWebServer handlers run on the async_tcp task. Calling the blocking
// form WiFi.scanNetworks() there starves the IDLE tasks and trips the
// watchdog (~4 s later, async_tcp reset). We use the async form instead:
//
//   status > 0  : results ready  -> serialize and return them
//   status -1   : scan in progress -> return {"scanning":true,"nets":[]}
//                 (JS retries every second until results appear)
//   otherwise   : no scan started yet -> kick one off, return scanning state
//
// JS treats "scanning":true as "show spinner, poll again in 1 s".
static String build_scan_json() {
    int status = WiFi.scanComplete();

    if (status == WIFI_SCAN_RUNNING || status == WIFI_SCAN_FAILED) {
        // Either currently scanning, or nothing started yet (-2). Either way,
        // make sure a scan is running and return the in-progress marker.
        if (status == WIFI_SCAN_FAILED) WiFi.scanNetworks(true); // async = true
        return String("{\"scanning\":true,\"nets\":[]}");
    }

    // status is the result count (>= 0) — serialize and free.
    DynamicJsonDocument doc(4096);
    doc["scanning"] = false;
    JsonArray arr = doc.createNestedArray("nets");
    for (int i = 0; i < status && i < 32; i++) {
        JsonObject net = arr.createNestedObject();
        net["ssid"]   = WiFi.SSID(i);
        net["rssi"]   = WiFi.RSSI(i);
        net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    String out; serializeJson(doc, out);
    return out;
}

// Called by every HTTP handler before doing real work — keeps the idle
// watchdog from firing while the user is mid-interaction.
static void bump_activity() { s_last_activity_ms = millis(); }

// ── Static request handlers ───────────────────────────────────────────────────
// Registered as plain function pointers (NOT lambdas) because
// ESPAsyncWebServer-esphome 3.4.x has reproducible crashes when iterating
// std::function-backed handlers on the first inbound request. Keeping these
// as static functions sidesteps that bug entirely.

// Serves the embedded portal HTML — also the target of every captive redirect.
// Scan is NOT started here; the user kicks it off explicitly with the Scan
// button so a refresh of the page doesn't waste airtime on an unwanted scan.
static void handle_root(AsyncWebServerRequest* req) {
    bump_activity();
    req->send_P(200, "text/html", PORTAL_HTML);
}

// GET /api/scan — runs a synchronous WiFi scan and returns the result as JSON.
static void handle_api_scan(AsyncWebServerRequest* req) {
    bump_activity();
    req->send(200, "application/json", build_scan_json());
}

// Empty header handler for POST routes — the real work happens in the body cb.
static void handle_post_noop(AsyncWebServerRequest* req) { (void)req; }

// POST /api/save body — parses the JSON, persists creds, acks, reboots.
static void handle_api_save_body(AsyncWebServerRequest* req,
                                 uint8_t* data, size_t len,
                                 size_t /*index*/, size_t /*total*/) {
    bump_activity();
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid JSON\"}");
        return;
    }
    const char* ssid = doc["ssid"] | "";
    const char* pass = doc["pass"] | "";
    if (!*ssid) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"ssid required\"}");
        return;
    }
    if (!wifi_creds_save_wifi(ssid, pass)) {
        req->send(500, "application/json", "{\"ok\":false,\"error\":\"flash write failed\"}");
        return;
    }
    req->send(200, "application/json", "{\"ok\":true}");
    Serial.println(F("portal: credentials saved — rebooting in 1s"));
    delay(1000);
    ESP.restart();
}

// Catch-all — every unknown URL (including OS captive-portal probes for
// Android /generate_204, iOS /hotspot-detect.html, Windows /ncsi.txt, etc.)
// redirects to root so the setup page pops automatically.
static void handle_not_found(AsyncWebServerRequest* req) {
    bump_activity();
    req->redirect("/");
}

// ── Public API ────────────────────────────────────────────────────────────────

bool portal_should_enter() {
    // 1. RPC path — set by enter_portal handler in the previous boot.
    if (wifi_creds_get_pending_portal()) {
        Serial.println(F("portal: pending_portal flag set — entering portal mode"));
        return true;
    }

    // 2. Physical button — PIN_PROGRAM held LOW for >= PORTAL_BUTTON_HOLD_MS.
    //    INPUT_PULLUP so a floating pin reads HIGH and we never accidentally enter.
    pinMode(PIN_PROGRAM, INPUT_PULLUP);
    if (digitalRead(PIN_PROGRAM) != LOW) return false;

    Serial.println(F("portal: PIN_PROGRAM held — hold for 3s to enter portal..."));
    uint32_t start = millis();
    while (digitalRead(PIN_PROGRAM) == LOW) {
        if (millis() - start >= PORTAL_BUTTON_HOLD_MS) {
            Serial.println(F("portal: button hold confirmed — entering portal mode"));
            return true;
        }
        delay(20);
    }
    Serial.println(F("portal: button released early — booting normally"));
    return false;
}

void portal_run() {
    // Build a per-device SSID so two units in the same room don't collide.
    uint8_t mac[6]; WiFi.macAddress(mac);
    char apSsid[32];
    snprintf(apSsid, sizeof(apSsid), DEFAULT_PORTAL_AP_PREFIX "%02X%02X", mac[4], mac[5]);

    // AP_STA keeps the STA radio alive so WiFi.scanNetworks() returns real results.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid, PORTAL_AP_PASSWORD);
    Serial.printf("portal: AP up — SSID='%s' pass='%s' ip=%s\n",
        apSsid, PORTAL_AP_PASSWORD, WiFi.softAPIP().toString().c_str());

    // Captive DNS — every host name resolves to the AP IP so the phone's
    // captive portal detector pops the page open automatically.
    dnsServer.start(53, "*", WiFi.softAPIP());

    // ── Routes (static handlers, no lambdas — see comment above) ──────────────
    // Single content route — every other URL (root, captive probes, anything
    // else) falls through to onNotFound -> redirect to "/". This sidesteps a
    // crash in ESPAsyncWebServer-esphome 3.4 when multiple lambdas share state.
    server.on("/",         HTTP_GET, handle_root);
    server.on("/api/scan", HTTP_GET, handle_api_scan);
    server.on("/api/save", HTTP_POST, handle_post_noop, nullptr, handle_api_save_body);
    server.onNotFound(handle_not_found);

    server.begin();

    // The pending_portal flag was honored by entering this function — clear it
    // now so a spurious reboot during the portal session doesn't loop back in.
    // (save_wifi clears it too on the success path; this handles the timeout
    // and unexpected-reset paths.)
    if (wifi_creds_get_pending_portal()) wifi_creds_set_pending_portal(false);

    Serial.println(F("portal: ready — waiting for client (10 min idle timeout)"));
    s_last_activity_ms = millis();

    // Single-threaded portal loop. DNS must be pumped explicitly; the web
    // server runs on its own AsyncTCP task so we only need a small yield.
    for (;;) {
        dnsServer.processNextRequest();
        if (millis() - s_last_activity_ms > PORTAL_IDLE_TIMEOUT_MS) {
            Serial.println(F("portal: idle timeout reached — rebooting to normal mode"));
            delay(200);
            ESP.restart();
        }
        delay(10);
    }
}
