#include "ConfigManager.h"
#include "default_config.h"

// System configuration functions
void ConfigManager::saveSystemConfig(const systemConfigTypedef_struct &config) {
	File configFileR = LittleFS.open("/system_config.json", FILE_READ);
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
	File configFile = LittleFS.open("/system_config.json", FILE_WRITE);
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
	File configFile = LittleFS.open("/system_config.json", FILE_READ);
	if (!configFile) {
		Serial.println("Failed to open system config file for reading.");
		file_creat = true;
		
	}
	if (file_creat)
	{
		file_creat=false;
		writeDefaultSystemConfig();
	}
	
	configFile = LittleFS.open("/system_config.json", FILE_READ);
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

  Serial.println(F("wifipass_sta: <hidden>"));  // password kept out of the serial log

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
	File configFile = LittleFS.open("/system_config.json", FILE_WRITE);
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
	
	doc["http_username"] = DEFAULT_HTTP_USER;
	doc["http_password"] = DEFAULT_HTTP_PASS;
	doc["wifissid_ap"] = DEFAULT_CFG_AP_SSID;
	doc["wifissid_sta"] = DEFAULT_WIFI_SSID;   // single source of truth (see default_config.h)
	doc["wifipass_ap"] = DEFAULT_CFG_AP_PASS;
	doc["wifipass_sta"] = DEFAULT_WIFI_PASS;
	doc["server_url"] = "";
	doc["wifi_reconnect_time"] = DEFAULT_WIFI_RECONNECT_S,
	doc["updates_interval"] = DEFAULT_UPDATES_INTERVAL_S,
	doc["realTime"] = DEFAULT_REALTIME;
	doc["server_port"] = DEFAULT_SERVER_PORT;
	doc["device_token"] = "";
	doc["device_location"] = DEFAULT_DEVICE_LOCATION;
	doc["sewing_machine_type"] = DEFAULT_MACHINE_TYPE;
	doc["operation_name"] = DEFAULT_OPERATION_NAME;
	doc["machine_serial"] = DEFAULT_MACHINE_SERIAL;
	doc["preScale"] = DEFAULT_PRESCALE;

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
	File configFile = LittleFS.open("/system_data.json", FILE_WRITE);
	if (!configFile) {
		Serial.println("Failed to open system config file for writing.");
		return;
	}

	// The ABSOLUTE counters are what gets persisted now. Previously this file
	// stored only the D*/T* variants and never the absolute ones, so a reboot
	// silently zeroed lifetime production — the opposite of what we want from a
	// monotonic counter that ThingsBoard windows.
	StaticJsonDocument<256> doc;
	doc["productionCounter"] = config.productionCounter;
	doc["powerTime"]         = config.powerTime;
	doc["runTime"]           = config.runTime;
	doc["count_total"]       = config.count_total;

	// Serialize the JSON document to the file.
	serializeJson(doc, configFile);

	configFile.close();
	Serial.println("System data saved.");
}

void ConfigManager::loadSystemData(systemDataTypedef_struct &strData) {
	bool file_creat = false;
	if (LittleFS.exists("/system_data.json")) {
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
	
	
	File configFile = LittleFS.open("/system_data.json", FILE_READ);
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

	// `| 0` so a file written by an older build — which had none of these keys —
	// loads as zero rather than garbage.
	strData.productionCounter = doc["productionCounter"] | 0;
	strData.powerTime         = doc["powerTime"]         | 0;
	strData.runTime           = doc["runTime"]           | 0;
	strData.count_total       = doc["count_total"]       | 0;

	Serial.print(F("Production counter: "));
	Serial.println(strData.productionCounter);
	Serial.print(F("Power on time: "));
	Serial.println(strData.powerTime);
	Serial.print(F("Run time: "));
	Serial.println(strData.runTime);
	Serial.print(F("Session count: "));
	Serial.println(strData.count_total);


	configFile.close();
	Serial.println("System Data loaded.");
}

void ConfigManager::writeDefaultSystemData() {
	File configFile = LittleFS.open("/system_data.json", FILE_WRITE);
	if (!configFile) {
		Serial.println("Failed to open system config file for writing.");
		return;
	}
	// Create a JSON document and set default values for the system configuration settings.
	StaticJsonDocument<256> doc;
	doc["productionCounter"] = 0;
	doc["powerTime"]         = 0;
	doc["runTime"]           = 0;
	doc["count_total"]       = 0;

	// Serialize the JSON document to the file.
	serializeJson(doc, configFile);
	configFile.close();
	Serial.println("Default system data file written.");
}
