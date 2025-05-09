
#include "tbBroker.h"

const char* fw_ver = "4.0";

const char* mqttBaseTopic = "nodered/sewing/";
// Device-specific topics
const char* authRequestTopic = "auth/request";
const char* authResponseTopic = "auth/response";
 // Construct the complete topic for this machine

TimerSW Timer_busy;
uint8_t currentState = 0;
uint8_t prevState = 10;
bool tbConnected = false;
bool authenticated = false;
bool oniline_msg_possible = false;
volatile bool ledState;
TimerSW Timer_tb_demon;
TimerSW Timer_tb_update;
TimerSW Timer_auth_request;
TimerSW Timer_mqtt_server_response;
EventGroupHandle_t EventRTOS_IO_events;

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
	 Timer_auth_request.previousMillis = millis();
	 Timer_auth_request.interval = 5000;
	 Timer_mqtt_server_response.interval = 5000;
	 Timer_mqtt_server_response.previousMillis = millis();
}



void mqtt_live(){
	static uint8_t mqtt_status=0;
	switch (mqtt_status)
	{
	case 0:{
		if (WiFi.status() != WL_CONNECTED){authenticated = false; oniline_msg_possible = false; return;}
		else{
			if(!client.connected()){
				authenticated = false; oniline_msg_possible = false;
			mqtt_status = 1;
			}
			else{
				if(!authenticated){
					mqtt_status=3;
					Serial.println(F("MQTT alreday connected"));
				}				
			}
		}
	}
		break;
	case 1:{
			oniline_msg_possible = false;
			tbConnected = false;
#ifndef IOT_PULSE_X
			digitalWrite(PIN_ONLINE,LOW);
#endif
			client.setKeepAlive(60);
			uint16_t port_number = atoi(structSysConfig.server_port);
			client.setServer(structSysConfig.server_url, port_number);
			client.setCallback(callback);			
			mqtt_status = 2;
		
	}
		break;
	case 2:{
			char mqttTopic[50] = {0};
			strcpy(mqttTopic,mqttBaseTopic);
			strcat(mqttTopic,device_id_macStr);
			 // prepare retain msg when device goes offline
			DynamicJsonDocument doc(512);  
			doc["id"] = device_id_macStr;
			doc["friendly_name"] = structSysConfig.friendly_name;
			char IP[] = "xxx.xxx.xxx.xxx";      
						IPAddress ip = WiFi.localIP();
						String my_ip = ip.toString();
			doc["ip"] = my_ip.c_str();
			doc["mac"] = macStr;
			doc["status"] = "offline";
			String jsonString;
			serializeJson(doc, jsonString);
			Serial.printf_P(PSTR("MQTT will:%s\n"), jsonString.c_str());
			Serial.printf_P(PSTR("MQTT connecting:%s port:%s \n"), structSysConfig.server_url,structSysConfig.server_port);
			//boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
			client.connect(device_id_macStr, "dhanushkadx", "cyclone10153", mqttTopic, 1, true, jsonString.c_str());
			mqtt_status = 3;			
	}
		break;	
	case 3:{
			Serial.println(F("MQTT Wait for responce"));
			vTaskDelay(1000 / portTICK_RATE_MS);
			if(client.connected()){
				Serial.println(F("MQTT connected"));			
#ifndef IOT_PULSE_X
			digitalWrite(PIN_ONLINE,HIGH);
#endif
		 // Subscribe to the authentication response topic
		 //dynamically generate auth topic for the device
			char mqttTopic_sub [100];
			strcpy(mqttTopic_sub,mqttBaseTopic);
			strcat(mqttTopic_sub,authResponseTopic);
			strcat(mqttTopic_sub,"/");
			strcat(mqttTopic_sub,device_id_macStr);
			
			Serial.printf_P(PSTR("MQTT Subscribe Topic:%s \n"),mqttTopic_sub);
			client.subscribe(mqttTopic_sub);
			mqtt_auth_request();
			Timer_auth_request.previousMillis = millis();
			mqtt_status = 4;
			}
			if(Timer_mqtt_server_response.Timer_run()){
				mqtt_status = 0;
			}
			
	}
		break;
	case 4:{
			if(authenticated){
				Serial.println(F("MQTT AUTH Ok"));
				if(!processOfflineMessagesV2()){
				oniline_msg_possible = true;
				tbConnected = true;
				mqtt_status = 0;
				}
				else{
					oniline_msg_possible = false;
					mqtt_status = 0;
					}
			}
			else if(Timer_auth_request.Timer_run()){
				Timer_auth_request.previousMillis = millis();
				Serial.println(F("MQTT AUTH FAILD"));
				client.disconnect();
				mqtt_status = 0;
				authenticated = false;
			}
	}
		break;
	default:
		break;
	}
}
				

void live_loop(){
	 mqtt_live();
	 //wifi_live(); used wifi event lib
	 client.loop(); 
	
}
 
 void com_loop() {	 
	 // wifi_live();	  
	  //tb_live();
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
	 doc["friendly_name"] = structSysConfig.friendly_name;
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
	
	  // Serialize the JsonDocument to a string
	  if(sendJson){
		if(!oniline_msg_possible ){Serial.println(F("AUTH Faild")); return;}
		send_data_to_tb();
		delay(1000);
		sendJson = false;
		String jsonString;
		Serial.print(F("online JSON Sending>"));
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

bool mqtt_auth_request(){
	char mqttTopic_pub [100];
	strcpy(mqttTopic_pub,mqttBaseTopic);
	strcat(mqttTopic_pub,authRequestTopic);
	strcat(mqttTopic_pub,"/");
	strcat(mqttTopic_pub,device_id_macStr);
	Serial.printf_P(PSTR("MQTT AUTH Publish Topic:%s "),mqttTopic_pub);

	File configFile = SPIFFS.open("/system_config.json", FILE_READ);
				// Create a JSON document and deserialize the system config data from the file to it.			
				DynamicJsonDocument jsonDoc(1024);
				DeserializationError error = deserializeJson(jsonDoc, configFile);
				if (error) {
					Serial.println(F("Failed to deserialize system config."));
					configFile.close();
					return true;
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
				//Serial.println(jsonString.c_str());
				// Publish the JSON payload to the MQTT topic
				if ( client.publish(mqttTopic_pub, jsonString.c_str())) {
					Serial.println(F("Ok"));
					return false;
				} else {
					Serial.println(F("Failed"));
					return true;
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
 //printLocalTime();

 while(!(xSemaphoreTake( xMutex_dataTB, portMAX_DELAY )));
 unsigned int DproductionCounter_local = structSysData.DproductionCounter;
 unsigned int DpowerTime_local = structSysData.DpowerTime;
 unsigned int DrunTime_local = structSysData.DrunTime; 
 xSemaphoreGive(xMutex_dataTB);	
 
  DynamicJsonDocument doc(200);  // Adjust the size based on your JSON structure
  
  // Set values in the JSON document
  //int realProductionCountAbsolute = (random(1,5)*10);
  //productionCounter_local = (realProductionCountAbsolute_prev + realProductionCountAbsolute);
  //realProductionCountAbsolute_prev = realProductionCountAbsolute_prev + realProductionCountAbsolute;
  doc["msgTyp"] = "update";
  doc["pwoT"] = DpowerTime_local;
  doc["DProductionCount"] = DproductionCounter_local;
  doc["runT"] = DrunTime_local;
  doc["id"] = device_id_macStr;
  //doc["friendly_name"] = structSysConfig.friendly_name;
  long rssi = WiFi.RSSI();
  doc["rssi"] =  rssi;
  doc["ts"] = getUTC_time();  // Add the UTC timestamp
#ifdef THERMO_OK
	doc["temp"] = get_temperatureC();
#endif
  ConfigManager :: saveSystemData(structSysData);

  // Serialize the JsonDocument to a string
  String jsonString;
  serializeJson(doc, jsonString);
  //sned data when network is available or save
  if (!oniline_msg_possible) {      
		saveMessageToSPIFFSV3(doc);
	} 
	else{
		Serial.println(jsonString.c_str());
  // Publish the JSON payload to the MQTT topic
  char mqttTopic [50];
  strcpy(mqttTopic,mqttBaseTopic);
  strcat(mqttTopic,device_id_macStr);
  if ( client.publish(mqttTopic, jsonString.c_str())) {
    Serial.println(F("JSON payload sent successfully"));
  } else {
    Serial.println(F("Failed to send JSON payload"));
	saveMessageToSPIFFSV3(doc);
  }  	

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



// Callback to handle incoming MQTT messages
void callback(char* topic, byte* payload, unsigned int length) {

  String receivedMessage;
  for (int i = 0; i < length; i++) {
    receivedMessage += (char)payload[i];
  }
  Serial.println(F("Callback>"));
  Serial.println(receivedMessage);
  // Parse the payload as JSON
  StaticJsonDocument<200> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, payload, length);

  if (error) {
    Serial.println(F("Failed to parse JSON"));
    return;
  }

  //dynamically generate auth topic for the device
	char mqttTopic_sub [100];
	strcpy(mqttTopic_sub,mqttBaseTopic);
	strcat(mqttTopic_sub,authResponseTopic);
	strcat(mqttTopic_sub,"/");
	strcat(mqttTopic_sub,device_id_macStr);
  // Handle authentication response
  if (String(topic) == mqttTopic_sub) {
    const char* responseDeviceId = jsonDoc["device_id"];
    const char* responseStatus = jsonDoc["status"];

    // Check if the response is for this device
    if (responseDeviceId && String(responseDeviceId) == device_id_macStr) {
      if (responseStatus && String(responseStatus) == "auth_success") {
		authenticated = true;
        Serial.println(F("Authentication successful!"));
        // Proceed with normal operations
      } else if (responseStatus && String(responseStatus) == "auth_fail") {
        Serial.println(F("Authentication failed. Taking action."));
        // Handle failure (e.g., retry, enter a safe state, etc.)
      } else {
        Serial.println(F("Unknown response status"));
      }
    } else {
      Serial.println(F("Response not for this device"));
    }
  }
}


