// CONFIGMANAGER.h

#ifndef _CONFIGMANAGER_h
#define _CONFIGMANAGER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#include <FS.h>
#include <SPIFFS.h>
#include "typex.h"
#include <ArduinoJson.h>
#include "config.h"

class ConfigManager {
	public:
	// System configuration functions
	static void saveSystemConfig(const systemConfigTypedef_struct &config);
	static void loadSystemConfig(systemConfigTypedef_struct &config);
	static void writeDefaultSystemConfig();

	// User data functions
	static void saveSystemData(const systemDataTypedef_struct &data);
	static void loadSystemData(systemDataTypedef_struct &data);
	static void writeDefaultSystemData();
};

#endif

