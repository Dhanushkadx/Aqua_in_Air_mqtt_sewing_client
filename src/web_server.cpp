// 
// 
// 

#include "web_server.h"


/*
    Name:       ESP32_websocket.ino
    Created:	11/22/2022 8:06:05 PM
    Author:     DARKLOAD\dhanu
*/

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>



int status_wifi;
bool wifiStarted = false;
TimerSW Timer_WIFIrecon;
// the IP address for the shield:
// Set your Static IP address
IPAddress local_IP(10, 0, 0, 100);
//IPAddress local_IP(192,168,43,10);
// Set your Gateway IP address
IPAddress gateway(10, 0, 0, 1);

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
byte bssid[] = {0xac, 0x71, 0x2e, 0x2e, 0xcc, 0x1a};
long previousMillis =0;
long interval = 30000;
#define HTTP_PORT 80

#define JSON_DOC_SIZE_DEVICE_DATA 15000
#define JSON_DOC_SIZE_USER_DATA 2048
StaticJsonDocument<JSON_DOC_SIZE_DEVICE_DATA> docz;

AsyncWebServer server(HTTP_PORT);


void initWebServerTimers(){
	Timer_WIFIrecon.interval = 60000;
	
}

void initWebServices()
{
	Serial.println(F("setting web socket"));
	//initSPIFFS();
	//initWiFi_AP();
	//WiFi.begin(WIFI_SSID, WIFI_PASS);
	initWebSocket();
	initWebServer();
	

}

void cleanClients(){
	ws.cleanupClients();
}



void initWebSocket() {
	ws.onEvent(onEvent);
	server.addHandler(&ws);
}


void initWiFi_STA(){
	WiFi.mode(WIFI_STA);
	// Configures static IP address
	if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
		Serial.println(F("STA Failed to configure"));
	}
	#ifdef FORCE_BSSID
		WiFi.begin(structSysConfig.wifipass_sta, structSysConfig.wifipass_sta,6,bssid);
		#else
		WiFi.begin(structSysConfig.wifissid_sta, structSysConfig.wifipass_sta);
		#endif
	Serial.printf("Trying to connect [%s] ", WiFi.macAddress().c_str());
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.printf(" %s\n", WiFi.localIP().toString().c_str());
}

void initWiFi_AP() {
	
	
	// Connect to Wi-Fi network with SSID and password
	Serial.println("Setting AP (Access Point)");
	// get the ESP32's MAC address
	uint8_t mac[6];
	WiFi.macAddress(mac);

	// convert the MAC address to a char string
	char macStr[18];  // buffer to hold the MAC address string
	sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	// set the WiFi AP name to "BLACK_WIRE_" followed by the MAC address string
	char apName[30];  // buffer to hold the WiFi AP name
	sprintf(apName, "BLACK_WIRE_%s", macStr);
	WiFi.softAP(apName);

	// print the WiFi AP name
	Serial.print("WiFi AP name: ");
	Serial.println(apName);
	//WiFi.softAP(wifi_ap_name, systemConfig.wifipass, 10, 0, 2);


	IPAddress IP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(IP);
	
}

void wifi_live() {
	// Loop until we're reconnected
	Timer_WIFIrecon.previousMillis = millis();

	if (WiFi.status() != WL_CONNECTED) {
		wifiStarted = false;
		WiFi.disconnect();
		vTaskDelay(3000 / portTICK_RATE_MS);
		/*if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
			Serial.println("STA Failed to configure");
		}*/
		Serial.printf("Connecting to %s...", structSysConfig.wifissid_sta);
		#ifdef FORCE_BSSID
		WiFi.begin(structSysConfig.wifipass_sta, structSysConfig.wifipass_sta,6,bssid);
		#else
		WiFi.begin(structSysConfig.wifissid_sta, structSysConfig.wifipass_sta);
		#endif
		//WiFi.waitForConnectResult();
		while (WiFi.status() != WL_CONNECTED) {
			Serial.print(".");
			vTaskDelay(500 / portTICK_RATE_MS);
			if (Timer_WIFIrecon.Timer_run()) {
				Serial.println("WiFi connection timeout, restarting...");
				WiFi.disconnect();
				ESP.restart();
				return;
			}
		}
		Serial.printf("\nConnected to %s\n", structSysConfig.wifissid_sta);
		delay(3000);
		char IP[] = "xxx.xxx.xxx.xxx";          // buffer
		IPAddress ip = WiFi.localIP();
		String my_ip = ip.toString();
		Serial.print(F("IP: "));
		Serial.println(my_ip.c_str());
		initRTC();
		wifiStarted = true;
	}
	
}

void initSPIFFS() {
	Serial.println(F("init SPIFF"));
	if (!SPIFFS.begin()) {
		Serial.println("Cannot mount SPIFFS volume...");
		while (1) {
			delay(100);
		}
	}
	
	
}

// ----------------------------------------------------------------------------
// Web server initialization
// ----------------------------------------------------------------------------

String processor(const String &var) {
	return String(var == "STATE" ? "on" : "off");
}

void onRootRequest(AsyncWebServerRequest *request) {
	 if(!request->authenticate( structSysConfig.http_username, structSysConfig.http_password))
	 return request->requestAuthentication();
	 
	 String path = request->url();
	 if(path == "/") {
		 path = "/index.html";
	 }

	 request->send(SPIFFS, path, "text/html", false, processor);
}

 // Send a GET request to <ESP_IP>/get?inputString=<inputMessage>
void onGetRequest(AsyncWebServerRequest *request) {
	String inputMessage;
	char buffer[50];
	if (request->hasParam("wifissid_sta")) {// I need to know the source web page of the GET request if this para available it s mean page is zone page
		File fileToReadx = SPIFFS.open("/system_config.json") ;
		DynamicJsonDocument docrx(JSON_DOC_SIZE_DEVICE_DATA);
		deserializeJson(docrx,  fileToReadx);
		fileToReadx.close();
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("wifissid_sta"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}
		
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("wifipass_sta"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}
		
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("server_url"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}
		
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("device_token"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}
		
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("server_port"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}
		
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("wifi_reconnect_time"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}

		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("updates_interval"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}
		
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("system_pass"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}
		
		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("device_location"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}

		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("sewing_machine_type"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}

		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("operation_name"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}

		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("serial_no"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}

		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("assest_no"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}

		memset(buffer,'\0',50 );
		strcpy_P(buffer,PSTR("barcode_no"));
		if (request->hasParam(buffer)) {
			inputMessage = request->getParam(buffer)->value();
			docrx[buffer]= inputMessage;
		}

		File fileToWritex = SPIFFS.open("/system_config.json", FILE_WRITE);		
		serializeJson(docrx,  fileToWritex);
		fileToWritex.close();
	}
	// reload config
	request->send(200, "text/text", "OK");
}	


void initWebServer() {
	server.on("/", onRootRequest);
	server.on("/get", onGetRequest);
	server.serveStatic("/", SPIFFS, "/");
	//server.setAuthentication(http_username, systemConfig.installer_pass);
	AsyncElegantOTA.begin(&server);    // Start ElegantOTA
	server.begin();
	/*server
	.serveStatic("/", SPIFFS, "/www/")
	.setDefaultFile("default.html")
	.setAuthentication("user", "pass");*/
}

void autoReconnect_loop() {
	unsigned long currentMillis = millis();
	// if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
	if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
		Serial.print(millis());
		Serial.println("Reconnecting to WiFi...");
		WiFi.disconnect();
		WiFi.reconnect();
		previousMillis = currentMillis;
	}
}


