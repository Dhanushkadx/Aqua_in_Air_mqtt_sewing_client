/*
 * statments.h
 *
 * Created: 12/13/2022 8:38:34 AM
 *  Author: DhanushkaC
 */ 


#ifndef STATMENTS_H_
#define STATMENTS_H_

//#define FORCE_BSSID
//#define THERMO_OK
#include "typex.h"
#include "WiFi.h"
#include "TimerSW.h"
#include "PubSubClient.h"
extern char macStr[18];
extern char device_id_macStr[18];
extern bool wifiStarted;
extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress primaryDNS;
extern IPAddress secondaryDNS;
extern WiFiClient espClient;
extern PubSubClient client;
extern systemConfigTypedef_struct structSysConfig;
extern systemDataTypedef_struct structSysData;
extern SemaphoreHandle_t  xMutex_dataTB;
extern bool configMode_enable;

extern volatile bool ledState;
extern bool busy;
extern bool Prev_faulty_alarm_status;
extern bool Current_faulty_alarm_status;
extern bool prev_actRun, actRun;
extern bool data_updated_timeSeries;
extern bool data_updated_critical;
extern eMC_state prevMCstate;
extern eMC_state curruntMCstate;
extern TimerSW Timer_powerOnTimer;
extern TimerSW Timer_runTimer;
extern TimerSW Timer_acNoice;
extern TimerSW Timer_idle_detect;
extern EventGroupHandle_t EventRTOS_IO_events;


#define sensor_pin_count 3





#define SIZE_StringToSend 100


/* define event bits */
#define TASK_1_BIT  ( 1 << 0 ) //1
#define TASK_2_BIT  ( 1 << 1 ) //10
#define TASK_3_BIT  ( 1 << 2 ) //100
#define TASK_4_BIT  ( 1 << 3 ) //1000
#define TASK_5_BIT  ( 1 << 4 ) //10000
#define TASK_6_BIT  ( 1 << 5 ) //100000
#define TASK_7_BIT  ( 1 << 6 ) //100000
#define ALL_SYNC_BITS (TASK_1_BIT | TASK_2_BIT | TASK_3_BIT | TASK_4_BIT | TASK_5_BIT | TASK_6_BIT | TASK_7_BIT) //111







#endif /* STATMENTS_H_ */