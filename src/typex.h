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
	char friendly_name[50];
	char location[50];
	
	//char device_name[25];
	//char device_lable[50];
	//char device_discriptor[50];
	
} systemConfigTypedef_struct;

typedef struct systemData{
	unsigned int productionCounter;
	unsigned int TproductionCounter;
	unsigned int DproductionCounter;

	unsigned int powerTime;
	unsigned int TpowerTime;
	unsigned int DpowerTime;

	unsigned int runTime=0;
	unsigned int TrunTime=0;
	unsigned int DrunTime=0;
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