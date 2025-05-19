
#include "tbBroker.h"
char* fw_ver = "1.50";

char* mqttBaseTopic = "nodered/sewing/";
 // Construct the complete topic for this machine

TimerSW Timer_busy;
bool tbConnected = false;
volatile bool ledState;
TimerSW Timer_tb_demon;
TimerSW Timer_tb_update;
EventGroupHandle_t EventRTOS_IO_events;
TimerSW Timer_mqtt_reconnect;
// Initialize underlying client, used to establish a connection
PubSubClient client(espClient);

//bool busy=false;
bool Prev_faulty_alarm_status = false;
bool Current_faulty_alarm_status = false;
bool prev_actRun= false,actRun = false;
bool data_updated_timeSeries= true;
bool data_updated_critical =true;
eMC_state prevMCstate = UNK; eMC_state curruntMCstate=IDLE;
int realProductionCountAbsolute_prev = 0;



void initTimers(){
	
	 Timer_tb_demon.interval=1000;
	 Timer_tb_demon.previousMillis-millis();
	 Timer_tb_update.interval= structSysConfig.updates_interval*1000;
	 Timer_tb_update.previousMillis = millis();
	 Timer_busy.interval = 1000;
	 Timer_busy.previousMillis = millis();
	 Timer_mqtt_reconnect.previousMillis = millis();
	 Timer_mqtt_reconnect.interval =60000;
}


void tb_live(){
	int state =WiFi.status();
    	if(state !=WL_CONNECTED)
        {
            Serial.println(F("MQTT - No WiFi to Reconnect MQTT"));
            return;
        }
	if (!client.connected()) {
		tbConnected = false;
		digitalWrite(PIN_ONLINE,LOW);
		client.setKeepAlive(60);
		uint16_t port_number = atoi(structSysConfig.server_port);
		client.setServer(structSysConfig.server_url, port_number);
		Serial.printf_P(PSTR("MQTT - Connecting to server %s port: %d"),structSysConfig.server_url, port_number);
		Timer_mqtt_reconnect.previousMillis = millis();
    	while (!client.connected()) {         
			//clientId += String(WiFi.macAddress());
			char mqttTopic [50];
			strcpy(mqttTopic,mqttBaseTopic);
			strcat(mqttTopic,device_id_macStr);
			 // immideat changig data send
			DynamicJsonDocument doc(512);  // Adjust the size based on your JSON structure	
			doc["id"] = device_id_macStr;
			doc["friendly_name"] = structSysConfig.friendly_name;
			char IP[] = "xxx.xxx.xxx.xxx";          // buffer
						IPAddress ip = WiFi.localIP();
						String my_ip = ip.toString();
			doc["ip"] = my_ip.c_str();
			doc["mac"] = macStr;
			doc["status"] = "offline";
			String jsonString;
			
			serializeJson(doc, jsonString);
			Serial.printf_P(PSTR("MQTT - Will message: %s \n"), jsonString.c_str());
			Serial.printf_P(PSTR("MQTT - Client ID: %s\n"),device_id_macStr);
			//boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
			if (client.connect(device_id_macStr, "dhanushkadx", "cyclone10153", mqttTopic, 1, true, jsonString.c_str())) {
				Serial.println(F("MQTT - connected"));
				digitalWrite(PIN_ONLINE,HIGH);
				tbConnected = true;
				send_metaData_json();
				
			} else {
				Serial.print(F("MQTT - failed with state  "));
				Serial.println(client.state());
				Serial.println(F("reconnect..."));
				delay(2000);
				if(Timer_mqtt_reconnect.Timer_run()){
					Timer_mqtt_reconnect.previousMillis = millis();
					// save data on flash
	 				ConfigManager :: saveSystemData(structSysData);
					ESP.restart();
				}
				
			}
		
    	}				
			}	
}


 
 void com_loop() {	 
	  //wifi_live();	  
	  tb_live();
	  while(!(xSemaphoreTake( xMutex_dataTB, portMAX_DELAY )));	 
	  bool Current_faulty_alarm_status_local = Current_faulty_alarm_status;
	  eMC_state curruntMCstate_local = curruntMCstate;
	  bool actRun_local = actRun;	  
	  xSemaphoreGive(xMutex_dataTB);	 

	  if(structSysConfig.realTime){
		if (data_updated_timeSeries) {
					Serial.println(F("immediate send time series data"));
					send_data_to_tb();
					data_updated_timeSeries = false;
					
			}
	  }
	  else{
			if(Timer_tb_update.Timer_run()){
				Timer_tb_update.previousMillis = millis();
				if (data_updated_timeSeries) {
					Serial.println(F("send time serous data"));
					send_data_to_tb();
					data_updated_timeSeries = false;
					
			}
	 }

	  }	 

	  // immideat changig data send
	 DynamicJsonDocument doc(512);  // Adjust the size based on your JSON structure	
	 doc["msgTyp"] = "realTm";
	 doc["id"] = device_id_macStr;
	 doc["fw_ver"] = fw_ver;
	 doc["location"] = structSysConfig.location;
  	 doc["friendly_name"] = structSysConfig.friendly_name;
   	 char IP[] = "xxx.xxx.xxx.xxx";          // buffer
				IPAddress ip = WiFi.localIP();
				String my_ip = ip.toString();
     doc["ip"] = my_ip.c_str();
     doc["mac"] = macStr;
	 bool sendJson = false;
	 // check MC state changers for immediate data sending
	 if (prevMCstate!=curruntMCstate_local){
		 prevMCstate=curruntMCstate_local;
		 if (curruntMCstate_local==MC_BUSY)
		 {
			 //busy=true;
			 doc["status"] = "busy";
		 }
		 else if(curruntMCstate_local==IDLE){
			// busy=false;
			 doc["status"] = "idle";
		 }	 
		 else if(curruntMCstate_local==MC_FAULT){
			 
			 doc["status"] = "fault";
		 }
		 sendJson = true;
	 }
	 
	 if (prev_actRun!=actRun){
		 prev_actRun=actRun;
		  doc["act_run"] = actRun;
		  sendJson = true;
	 }
	 
	  /*if (Prev_faulty_alarm_status!=Current_faulty_alarm_status_local){
		  Prev_faulty_alarm_status=Current_faulty_alarm_status_local;
		  doc["fault"] = Prev_faulty_alarm_status;
		  sendJson = true;
	  }	 */

	  // Serialize the JsonDocument to a string
	  if(sendJson){
		send_data_to_tb();
		delay(1000);
		sendJson = false;
		String jsonString;
		Serial.print(F("JSON Sending>"));
		serializeJson(doc, jsonString);
		Serial.println(jsonString.c_str());
		// Publish the JSON payload to the MQTT topic
		char mqttTopic [50];
		strcpy(mqttTopic,mqttBaseTopic);
		strcat(mqttTopic,device_id_macStr);
		if ( client.publish(mqttTopic, jsonString.c_str())) {
			Serial.println(F("JSON payload sent successfully"));
		} else {
			Serial.println(F("Failed to send JSON payload"));
		} 

	  }
		
 }

 void dayBreakConuterReset(){
	// reset counters when dayBreak
	 if(wifiStarted){
		int today;
		getDate(&today);
	    		Serial.print(F("today is "));
				Serial.println(today);
			if(structSysConfig.yesterDay != today){
				Serial.print(F("today is "));
				Serial.println(today);
				Serial.print(F("   yesterday is "));
				Serial.println(structSysConfig.yesterDay);
				structSysConfig.yesterDay = today;
				Serial.print(F("day breakes and counter reset now yester day is:"));
				Serial.println(structSysConfig.yesterDay);
				ConfigManager::saveSystemConfig(structSysConfig);	
				structSysData.DpowerTime = 0;
				structSysData.DrunTime = 0;
				structSysData.DproductionCounter = 0;
				ConfigManager :: saveSystemData(structSysData);;	
			}
	 }
 }
 
 void send_data_to_tb(){
 printLocalTime();

 while(!(xSemaphoreTake( xMutex_dataTB, portMAX_DELAY )));
 unsigned int productionCounter_local = structSysData.productionCounter;
 unsigned int TproductionCounter_local = structSysData.TproductionCounter;
 unsigned int DproductionCounter_local = structSysData.DproductionCounter;

 unsigned int powerTime_local = structSysData.powerTime;
 unsigned int TpowerTime_local = structSysData.TpowerTime;
 unsigned int DpowerTime_local = structSysData.DpowerTime;

 unsigned int runTime_local = structSysData.runTime;
 unsigned int TrunTime_local = structSysData.TrunTime;
 unsigned int DrunTime_local = structSysData.DrunTime; 
 xSemaphoreGive(xMutex_dataTB);		

 
 
// Create a JSON document and deserialize the system config data from the file to it.
  DynamicJsonDocument doc(512);  // Adjust the size based on your JSON structure
  // Set values in the JSON document
  // check MC state changers for immediate data sending
		 if (prevMCstate==MC_BUSY)
		 {
			 //busy=true;
			 doc["status"] = "busy";
		 }
		 else if(prevMCstate==IDLE){
			// busy=false;
			 doc["status"] = "idle";
		 }	 
		 else if(prevMCstate==MC_FAULT){
			 
			 doc["status"] = "fault";
		 }
  doc["fw_ver"] = fw_ver;
  doc["msgTyp"] = "update";
  doc["PowerOn"] = powerTime_local;
  doc["DPowerOn"] = DpowerTime_local;
  doc["ProductionCount"] = productionCounter_local;
  doc["DProductionCount"] = DproductionCounter_local;
  doc["TProductionCount"] = TproductionCounter_local;
  doc["runTime"] = "runTime_local";
  doc["DrunTime"] = DrunTime_local;
  unsigned int Dlost_time = DpowerTime_local - DrunTime_local;
  doc["Dlost_time"] = Dlost_time;
  doc["id"] = device_id_macStr;
  doc["friendly_name"] = structSysConfig.friendly_name;
  char IP[] = "xxx.xxx.xxx.xxx";          // buffer
				IPAddress ip = WiFi.localIP();
				String my_ip = ip.toString();
  doc["ip"] = my_ip.c_str();
  doc["mac"] = macStr;
  long rssi = WiFi.RSSI();
  doc["rssi"] =  rssi;
#ifdef THERMO_OK
	doc["temp"] = get_temperatureC();
#endif
	// save data on flash
	 ConfigManager :: saveSystemData(structSysData);
  // Serialize the JsonDocument to a string
  String jsonString;
  Serial.print(F("JSON Sending>"));
  serializeJson(doc, jsonString);
  Serial.println(jsonString.c_str());
  // Publish the JSON payload to the MQTT topic
  char mqttTopic [50];
  strcpy(mqttTopic,mqttBaseTopic);
  strcat(mqttTopic,device_id_macStr);
  if ( client.publish(mqttTopic, jsonString.c_str())) {
    Serial.println(F("JSON payload sent successfully"));
  } else {
    Serial.println(F("Failed to send JSON payload"));
  }  	
	
 }


 void send_metaData_json(){
	File configFile = SPIFFS.open("/system_config.json", FILE_READ);
				// Create a JSON document and deserialize the system config data from the file to it.			
				DynamicJsonDocument jsonDoc(1024);
				DeserializationError error = deserializeJson(jsonDoc, configFile);
				if (error) {
					Serial.println(F("Failed to deserialize system config."));
					configFile.close();
					return;
				}	
				configFile.close();  	
				// Update shared attribute on Thingsboard server		
				char IP[] = "xxx.xxx.xxx.xxx";          // buffer
				IPAddress ip = WiFi.localIP();
				String my_ip = ip.toString();
				// Assign example values to the keys
				jsonDoc["msgTyp"] = "init";
				jsonDoc["id"] = device_id_macStr;
				jsonDoc["fw_ver"] = fw_ver;
				jsonDoc["ip"] = my_ip.c_str();
				jsonDoc["mac"] = macStr;
				jsonDoc["friendly_name"] = structSysConfig.friendly_name;
				jsonDoc["status"] = "idle";
				long rssi = WiFi.RSSI();
                jsonDoc["rssi"] =  rssi;
				// Serialize the JsonDocument to a string
				String jsonString;
				serializeJson(jsonDoc, jsonString);
				Serial.println(jsonString.c_str());
				// Publish the JSON payload to the MQTT topic
				char mqttTopic [50];
				strcpy(mqttTopic,mqttBaseTopic);
				strcat(mqttTopic,device_id_macStr);
				
				if ( client.publish(mqttTopic, jsonString.c_str())) {
					Serial.println(F("JSON payload sent successfully"));
				} else {
					Serial.println(F("Failed to send JSON payload"));
				} 
				  	
 }

