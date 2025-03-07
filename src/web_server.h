// web_server.h

#ifndef _WEB_SERVER_h
#define _WEB_SERVER_h

#include "typex.h"
#include "config.h"
#include "web_socket.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"

#include <ArduinoJson.h>
#include "TimerSW.h"
#include "clockConfig.h"


#define JSON_DOC_SIZE_DEVICE_DATA 15000
#define JSON_DOC_SIZE_USER_DATA 2048



extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress primaryDNS;
extern IPAddress secondaryDNS;
extern long previousMillis;
extern long interval;
extern const int HTTP_PORT;
extern StaticJsonDocument<JSON_DOC_SIZE_DEVICE_DATA> docz;
extern AsyncWebServer server;
extern AsyncWebSocket ws;

extern systemConfigTypedef_struct structSysConfig;
extern systemDataTypedef_struct structSysData;

void initWebServices();
void cleanClients();
void initWebSocket();
void initWebServer();
void initWiFi_AP();
void wifi_live();
void initSPIFFS();
void initWebServerTimers();
void onRootRequest(AsyncWebServerRequest *request);
void onGetRequest(AsyncWebServerRequest *request);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);  
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);

#endif

