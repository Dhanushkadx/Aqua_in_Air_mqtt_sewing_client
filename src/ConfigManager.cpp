#include "ConfigManager.h"

// System configuration functions
void ConfigManager::saveSystemConfig(const systemConfigTypedef_struct &config) {
	File configFileR = SPIFFS.open("/system_config.json", FILE_READ);
	if (!configFileR) {
		Serial.println("Failed to open system config file for reading after creating default.");
		return;
	}
	// Create a JSON document and deserialize the system config data from the file to it.
	//StaticJsonDocument<1556> doc;
	DynamicJsonDocument docR(1024);
	DeserializationError error = deserializeJson(docR, configFileR);
	if (error) {
		Serial.println("Failed to deserialize system config.");
		configFileR.close();
		return;
	}
	configFileR.close();
	///////////////////////////////////////////////////////////////////
	File configFile = SPIFFS.open("/system_config.json", FILE_WRITE);
	if (!configFile) {
		Serial.println("Failed to open system config file for writing.");
		return;
	}

	// Create a JSON document and serialize the system config data to it.
	StaticJsonDocument<1024> doc;
	doc["wifissid_ap"] = config.wifissid_ap;
	doc["wifissid_sta"] = config.wifissid_sta;
	doc["wifipass_ap"] = config.wifipass_ap;
	doc["wifipass_sta"] = config.wifipass_sta;
	doc["server_url"] = config.server_url;
	doc["server_port"] = config.server_port;
	doc["device_token"] = config.device_token;
	doc["wifi_reconnect_time"] = config.wifi_reconnect_time;
	doc["updates_interval"] = config.updates_interval;
	doc["yesterDay"] = config.yesterDay;
	doc["http_username"] = config.http_username;
	doc["http_password"] = config.http_password;
	doc["realTime"] = config.realTime;
	doc["preScale"] = config.preScale;

	doc["serial_no"] = docR["serial_no"];
	doc["assest_no"] = docR["assest_no"];
	doc["barcode_no"] = docR["barcode_no"];
	doc["system_pass"] = docR["system_pass"];
	doc["device_location"] = docR["device_location"];
	doc["sewing_machine_type"] = docR["sewing_machine_type"];
	doc["operation_name"] = docR["operation_name"];
 
	
  
 /*
 doc["system_pass"] = system_pass;
  doc["device_location"] = device_location;
  doc["operation_name"] = operation_name;
  doc["serial_no"] = machine_se
  doc["yesterDay"] =
 */ 
  
  
	
	// Serialize the JSON document to the file.
	serializeJson(doc, configFile);

	configFile.close();
	Serial.println("System config saved.");
}

void ConfigManager::loadSystemConfig(systemConfigTypedef_struct &config) {
	bool file_creat = false;
	File configFile = SPIFFS.open("/system_config.json", FILE_READ);
	if (!configFile) {
		Serial.println("Failed to open system config file for reading.");
		file_creat = true;
		
	}
	if (file_creat)
	{
		file_creat=false;
		writeDefaultSystemConfig();
	}
	
	configFile = SPIFFS.open("/system_config.json", FILE_READ);
	if (!configFile) {
		Serial.println("Failed to open system config file for reading after creating default.");
		return;
	}
	// Create a JSON document and deserialize the system config data from the file to it.
	//StaticJsonDocument<1556> doc;
	DynamicJsonDocument doc(1024);
	DeserializationError error = deserializeJson(doc, configFile);
	if (error) {
		Serial.println("Failed to deserialize system config.");
		configFile.close();
		return;
	}

	// Copy the system config data from the JSON document to the system config struct.
	/* "wifissid_ap": "",
  "wifissid_sta": "DarkNet",
  "wifipass_ap": "",
  "wifipass_sta": "xwelcomexxx",
  "server_url": "thingsboard.cloud",
  "device_token": "I5EOJaZX4jjc0duc5wPXd1",
  "server_port": "1883",
  "wifi_reconnect_time": 30,
  "updates_interval": 30,
  "system_pass": "1234",
  "http_username": "admin",
  "http_password": "admin",
  "yesterDay" : 1*/
	strcpy(config.http_username , doc["http_username"]);
	strcpy(config.http_password , doc["http_password"]);
	strcpy(config.wifissid_ap , doc["wifissid_ap"]);
	strcpy(config.wifissid_sta , doc["wifissid_sta"]);
	strcpy(config.wifipass_ap , doc["wifipass_ap"]);
	strcpy(config.wifipass_sta , doc["wifipass_sta"]);
	strcpy(config.server_url , doc["server_url"]);
	strcpy(config.server_port , doc["server_port"]);
	strcpy(config.device_token , doc["device_token"]);
	config.wifi_reconnect_time = doc["wifi_reconnect_time"];
	config.updates_interval = doc["updates_interval"];
	config.yesterDay = doc["yesterDay"];
	config.realTime = doc["realTime"];
	config.preScale = doc["preScale"];
	strcpy(config.friendly_name,doc["operation_name"]);
	strcpy(config.location,doc["device_location"]);
	
	//config.wifi_reconnect_time = 30000;
	Serial.print(F("wifissid_ap: "));
  Serial.println(doc["wifissid_ap"].as<String>());

  Serial.print(F("wifissid_sta: "));
  Serial.println(doc["wifissid_sta"].as<String>());

  Serial.print(F("wifipass_ap: "));
  Serial.println(doc["wifipass_ap"].as<String>());

  Serial.print(F("wifipass_sta: "));
  Serial.println(doc["wifipass_sta"].as<String>());

  Serial.print(F("server_url: "));
  Serial.println(doc["server_url"].as<String>());

  Serial.print(F("device_token: "));
  Serial.println(doc["device_token"].as<String>());

  Serial.print(F("server_port: "));
  Serial.println(doc["server_port"].as<String>());

  Serial.print(F("wifi_reconnect_time: "));
  Serial.println(doc["wifi_reconnect_time"].as<int>());

  Serial.print(F("updates_interval: "));
  Serial.println(doc["updates_interval"].as<int>());

  Serial.print(F("realTime: "));
  Serial.println(doc["realTime"].as<bool>());

  Serial.print(F("system_pass: "));
  Serial.println(doc["system_pass"].as<String>());

  Serial.print(F("http_username: "));
  Serial.println(doc["http_username"].as<String>());

  Serial.print(F("http_password: "));
  Serial.println(doc["http_password"].as<String>());

  Serial.print(F("device_location: "));
  Serial.println(doc["device_location"].as<String>());

  Serial.print(F("sewing_machine_type: "));
  Serial.println(doc["sewing_machine_type"].as<String>());

  Serial.print(F("operation_name: "));
  Serial.println(doc["operation_name"].as<String>());

  Serial.print(F("serial no: "));
  Serial.println(doc["serial_no"].as<String>());

  Serial.print(F("assest no: "));
  Serial.println(doc["assest_no"].as<String>());

  Serial.print(F("barcode no: "));
  Serial.println(doc["barcode_no"].as<String>());

  Serial.print(F("Yesterday: "));
  Serial.println(doc["yesterDay"].as<int>());

  Serial.print(F("preScale: "));
  Serial.println(doc["preScale"].as<int>());
	
	
	configFile.close();
	Serial.println("System config loaded.");
}

void ConfigManager::writeDefaultSystemConfig() {
	File configFile = SPIFFS.open("/system_config.json", FILE_WRITE);
	if (!configFile) {
		Serial.println("Failed to open system config file for writing.");
		//return;
	}
	
	DynamicJsonDocument doc(1556);
	
	//DeserializationError error = deserializeJson(doc, configFile);
	//if (error) {
	//	Serial.println("Failed to deserialize system config.");
	//	configFile.close();
	//	return;
	//}

	// Create a JSON document and set default values for the system configuration settings.
	//StaticJsonDocument<1556> doc;
	
	doc["http_username"] = "admin";
	doc["http_password"] = "admin";
	doc["wifissid_ap"] = "";
	doc["wifissid_sta"] = "";
	doc["wifipass_ap"] = "";
	doc["wifipass_sta"] = "";
	doc["server_url"] = "";
	doc["wifi_reconnect_time"] = 30,
	doc["updates_interval"] = 30,
	doc["realTime"] = false;
	doc["server_port"] = "1883";
	doc["device_token"] = "";
	doc["device_location"] = "module_";
	doc["sewing_machine_type"] = "single needle";
	doc["operation_name"] = "any operation";
	doc["machine_serial"] = "000000000000";
	doc["preScale"] = 1;

	// Calculate the required size to store the serialized JSON data
	//size_t jsonSize = measureJson(doc);

	// Allocate the DynamicJsonDocument object with the required size
	//DynamicJsonDocument serializedDoc(jsonSize);

	// Serialize the JSON document to the file
	serializeJsonPretty(doc, Serial);
	serializeJson(doc, configFile);

	configFile.close();
	Serial.println(F("Default system config file written."));
}

void ConfigManager::saveSystemData(const systemDataTypedef_struct &config) {
	File configFile = SPIFFS.open("/system_data.json", FILE_WRITE);
	if (!configFile) {
		Serial.println("Failed to open system config file for writing.");
		return;
	}

	// Create a JSON document and serialize the system config data to it.
	StaticJsonDocument<1256> doc;
	doc["DpowerTime"] = config.DpowerTime;
	doc["DrunTime"] = config.DrunTime;
	doc["TpowerTime"] = config.TpowerTime;
	doc["TrunTime"] = config.TrunTime;
	doc["DproductionCounter"] = config.DproductionCounter;
	doc["TproductionCounter"] = config.TproductionCounter;

	// Serialize the JSON document to the file.
	serializeJson(doc, configFile);

	configFile.close();
	Serial.println("System data saved.");
}

void ConfigManager::loadSystemData(systemDataTypedef_struct &strData) {
	bool file_creat = false;
	if (SPIFFS.exists("/system_data.json")) {
		Serial.println("system_data.json exists");
		} else {
		Serial.println("system_data.json does not exist");
		file_creat = true;
	}
	
	if (file_creat)
	{
		file_creat = false;
		writeDefaultSystemData();
	}
	
	
	File configFile = SPIFFS.open("/system_data.json", FILE_READ);
	if (!configFile) {
		Serial.println("Failed to open system data file for reading after creating default.");
		return;
	}

	// Create a JSON document and deserialize the system config data from the file to it.
	StaticJsonDocument<1256> doc;
	DeserializationError error = deserializeJson(doc, configFile);
	if (error) {
		Serial.println("Failed to deserialize system data.");
		configFile.close();
		return;
	}

	// Copy the system config data from the JSON document to the system config struct.
	strData.TpowerTime = doc["TpowerTime"];
	strData.DpowerTime = doc["DpowerTime"];
	strData.TrunTime = doc["TrunTime"];
	strData.DrunTime = doc["DrunTime"];
	strData.TproductionCounter = doc["TproductionCounter"];
	strData.DproductionCounter = doc["DproductionCounter"];

	Serial.print(F("Power ON time: "));
	Serial.println(strData.TpowerTime);
	Serial.print(F("Power OFF time: "));
	Serial.println(strData.DpowerTime);
	Serial.print(F("Production ON time: "));
	Serial.println(strData.TrunTime);
	Serial.print(F("Production OFF time: "));
	Serial.println(strData.DrunTime);
	Serial.print(F("Total production counter: "));
	Serial.println(strData.TproductionCounter);
	Serial.print(F("Daily production counter: "));
	Serial.println(strData.DproductionCounter);
	
	
	configFile.close();
	Serial.println("System Data loaded.");
}

void ConfigManager::writeDefaultSystemData() {
	File configFile = SPIFFS.open("/system_data.json", FILE_WRITE);
	if (!configFile) {
		Serial.println("Failed to open system config file for writing.");
		return;
	}
	// Create a JSON document and set default values for the system configuration settings.
	StaticJsonDocument<1256> doc;
	doc["TpowerTime"] = 0;
	doc["DpowerTime"] = 0;
	doc["TrunTime"] = 0;
	doc["DrunTime"] = 0;
	doc["TproductionCounter"] = 0;
	doc["DproductionCounter"] = 0;

	// Serialize the JSON document to the file.
	serializeJson(doc, configFile);
	configFile.close();
	Serial.println("Default system data file written.");
}
