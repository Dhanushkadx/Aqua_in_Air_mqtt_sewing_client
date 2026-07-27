// tbBroker.h

#ifndef _TBBROKER_h
#define _TBBROKER_h

//#define MQTT_SECURE

#include <Arduino.h>
#include "pinsx.h"
#include "typex.h"
#include "web_server.h"
#include "statments.h"
#include "ConfigManager.h"
#include <WiFi.h>
#include "TimerSW.h"
#include <LittleFS.h>
#include "clockConfig.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#ifdef THERMO_OK
#include "thermo.h"
#endif

// Statuses for updating
extern char* fw_ver;
extern bool updateRequestSent;
extern bool tbConnected; 
extern byte bssid[];

	void initTimers();
	void com_loop();
	void tb_live();

	void send_data_to_tb();
	void dayBreakConuterReset();
	 void send_metaData_json();


#endif // TB_BROKER_H


