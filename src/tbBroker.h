// tbBroker.h

#ifndef _TBBROKER_h
#define _TBBROKER_h
#include <Arduino.h>
#include "pinsx.h"
#include "typex.h"
#include "typex.h"
#include "web_server.h"
#include "statments.h"
#include "ConfigManager.h"
#include <WiFi.h>
#include "TimerSW.h"
#include <SPIFFS.h>
#include "clockConfig.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "msg_store.h"

#ifdef THERMO_OK
#include "thermo.h"
#endif

// Statuses for updating
extern char* fw_ver;
extern bool updateRequestSent;
extern bool tbConnected; 
extern byte bssid[];
extern PubSubClient client;

	void initTimers();
	void com_loop();
	bool mqtt_auth_request();
	void live_loop();
	void mqtt_live();
	void callback(char* topic, byte* payload, unsigned int length);
	void send_data_to_tb();
	void dayBreakConuterReset();
	 void send_metaData_json();


#endif // TB_BROKER_H


