#include "wifi_com.h"
#include "wifi_creds.h"
#include "../clockConfig.h"   // initRTC() — the sewing client starts NTP on GOT_IP
#include "domain_cmd.h"       // hand the save+restart to the task that owns it
#include "../sewing_cmd.h"    // SEW_CMD_SAVE_AND_RESTART
#include "../default_config.h"

// Compile-time WiFi seed — used by wifi_creds_init() on first boot, before
// /wifi.json exists. After the very first boot the file becomes the source of
// truth (operator updates via the portal). Edit the value in default_config.h.
const char* ssid     = DEFAULT_WIFI_SSID;
const char* password = DEFAULT_WIFI_PASS;

String hostname = "PulseX Sewing";
TimerSW Timer_WIFIreconnect;

//#define CUSTOM_NETWORK_CONFIG
// the IP address for the shield:
// Set your Static IP address
IPAddress local_IP(10, 0, 0, 100);
//IPAddress local_IP(192,168,43,10);
// Set your Gateway IP address
IPAddress gateway(10, 0, 0, 1);

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

#define HTTP_PORT 80

// wifi
// the Wifi radio's status
int status = WL_IDLE_STATUS;
bool wifiStarted = false;
// Owned here now (was web_server.cpp). Task4's LED pattern and loop()'s pixel
// state machine both read it, so it must track GOT_IP, not just association.
// volatile: written on the WiFi event task, read on loopTask (single writer,
// single-byte flag — atomic, no lock; volatile just guarantees a fresh read).
volatile bool wifiIPgot = false;
// Associated to the AP but not necessarily IP'd yet — the middle state the VERO
// status LED shows as a double-blink. Same cross-task single-writer contract.
volatile bool wifi_assoc = false;


void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
	Serial.println(F("Connected to AP successfully!"));
	wifi_assoc = true;
  }
  
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
	// Print the network we ACTUALLY associated with (from the radio), not the
	// compile-time default global — the active SSID comes from wifi_creds and
	// may differ from `ssid` once the portal has saved new credentials.
	Serial.printf_P(PSTR("\nConnected to %s\n"), WiFi.SSID().c_str());
		delay(3000);
		char IP[] = "xxx.xxx.xxx.xxx";          // buffer
		IPAddress ip = WiFi.localIP();
		String my_ip = ip.toString();
		Serial.print(F("IP: "));
		Serial.println(my_ip.c_str());
		initRTC();
		wifiStarted = true;
		wifiIPgot   = true;
		// WiFi state is shown by the status LEDs (led_status.cpp polls WiFi.status()).
  }
  
  void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
	Serial.println(F("Disconnected from WiFi access point"));
	Serial.print(F("Reason: "));
	Serial.println(info.wifi_sta_disconnected.reason);
	Serial.println(F("Trying to Reconnect WiFi"));
	WiFi.reconnect();
	if(wifiStarted){// Loop until we're reconnected
		Timer_WIFIreconnect.previousMillis = millis();
		wifiStarted = false;
	}
	wifiIPgot = false;
	wifi_assoc = false;   // dropped the AP — back to fast-blink "connecting"
			if (Timer_WIFIreconnect.Timer_run()) {
				// Persist the counters before the restart — an unsaved
				// productionCounter is real lost production for the plant.
				//
				// We do NOT save or reboot here. This runs on the system event
				// task, whose stack is ~2304 bytes by default, and
				// saveSystemData() puts a 1256-byte StaticJsonDocument on it.
				// Task2 owns the counters and has a 10000-byte stack, so it does
				// the save — and gets a consistent snapshot for free, since
				// nothing can increment them mid-serialise.
				Serial.println(F("WiFi connection timeout, requesting save + restart"));
				domain_cmd_post(SEW_CMD_SAVE_AND_RESTART);
				return;
			}
  }


void initWiFi_STA(){
    Timer_WIFIreconnect.interval = 60000; // 60 seconds timeout for WiFi reconnection
	WiFi.mode(WIFI_STA);
	WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
	WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
	WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
	#ifdef CUSTOM_NETWORK_CONFIG
	if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
		Serial.println(F("STA Failed to configure"));
	}
#endif
#ifdef FORCE_BSSID
		WiFi.begin(structSysConfig.wifipass_sta, structSysConfig.wifipass_sta,6,bssid);
#else
		// Read from /wifi.json (populated by the portal). Falls back to the
		// compile-time defaults on first boot via wifi_creds_init().
		const char* active_ssid = wifi_creds_get_ssid();
		const char* active_pass = wifi_creds_get_pass();
		WiFi.begin(active_ssid, active_pass);
		Serial.printf_P(PSTR("WiFi.begin(%s, %s)\n"), active_ssid, active_pass);
#endif
	Serial.printf_P(PSTR("Trying to connect [%s] "), wifi_creds_get_ssid());
	//uint32_t red = Adafruit_NeoPixel::Color(0, 0, 255);
  	//pixel.startBlink(red, 300, 300, 180);
}