/*
 * typex.h
 *
 * Created: 4/19/2023 7:54:42 AM
 *  Author: DhanushkaC
 */ 


#ifndef TYPEX_H_
#define TYPEX_H_
#include "Arduino.h"


typedef struct systemConfig{
	
	char wifissid_ap[15]; //":"dxdxdxdxdx",
	char wifissid_sta[15]; //":"dxdxdxdxdx",
	char wifipass_ap[15]; //": "xxxxxxxxxx",
	char wifipass_sta[15]; //": "xxxxxxxxxx",
	char server_url[100];
	char device_token[100];
	char server_port[10];
	char http_username[15];
	char http_password[15];
	uint32_t updates_interval;
	bool realTime;
	uint32_t wifi_reconnect_time;
	int yesterDay;
	uint16_t preScale;
	// Production input source: 0 = GPIO sensors (opto pulses), 1 = Modbus PLC
	// (mirror the machine's own counters over RS485). Selectable at runtime via
	// the web UI / RPC. Only honoured on HAS_MODBUS boards.
	uint8_t input_mode;
	char friendly_name[50];
	char location[50];
	
	//char device_name[25];
	//char device_lable[50];
	//char device_discriptor[50];
	
} systemConfigTypedef_struct;

// Absolute, monotonic counters. They never roll over on the device — the daily
// and shift figures are derived server-side by windowing the series in
// ThingsBoard, which is more robust than a device-side midnight reset (no clock
// dependency, no lost rollover if the device is off at midnight).
//
// They only change by: incrementing, an explicit counter_reset / set_counter
// RPC, or a factory reset. Persisted to /system_data.json so a reboot does not
// lose them. Written and read ONLY by Task2 — see sewing_cmd.h.
typedef struct systemData{
	unsigned int productionCounter;   // pieces produced, lifetime (odometer)
	unsigned int powerTime;           // seconds powered, lifetime
	unsigned int runTime;             // seconds running, lifetime
	// PrimeFlow session counter (doc/11C count_total). Pieces produced within the
	// CURRENT manifest — resets to 0 when the manifest_id changes (lifecycle R1).
	// Persisted too, so a reboot mid-manifest resumes the session count rather
	// than losing it. Owned by Task2 like the others.
	unsigned int count_total;
	}systemDataTypedef_struct;

typedef enum {MC_BUSY,IDLE,MC_FAULT,UNK}eMC_state;
	
typedef struct GPIO_data{
	uint8_t GPIOpin;
	bool prevState;
	bool nowState;
	void (*fn_RISE_EDGE)(int)=NULL;
	void (*fn_FALL_EDGE)(int) = NULL;
	void (*fn_LOW_CONTINU)(void)=NULL;
	void (*fn_HIGH_CONTINU)(void) = NULL;
	
	long last_changed_time;
} Struct_GPIO_INFO;

typedef struct SYS_data{
	long powerOnTime;
	long runTime;
	long productionCount;
} SYSCONFIG;
#endif /* TYPEX_H_ */