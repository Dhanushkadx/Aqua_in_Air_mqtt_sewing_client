// write gard header
#ifndef WIFI_COM_H
#define WIFI_COM_H  
#include "Arduino.h"
#include <WiFi.h>
#include "../pinsx.h"
#include "../TimerSW.h"


extern bool wifiStarted;


void initWiFi_STA();

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);
  
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
  
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);





#endif