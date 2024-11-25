/*
 * pinsx.h
 *
 * Created: 12/13/2022 8:37:35 AM
 *  Author: DhanushkaC
 */ 


#ifndef PINSX_H_
#define PINSX_H_

//#define PLC_IOT_BRIDGE

#ifdef PLC_IOT_BRIDGE
#define RELAY1 32
#define RELAY2 33
#define PIN_DOWNTIME_SWITCH 23
#define PIN_LED_FAULT RELAY1
#define PIN_ONLINE RELAY2
#define PIN_INPUT1 22//19
#define PIN_INPUT2 21//22
#define PIN_INPUT3 PIN_DOWNTIME_SWITCH
#define PIN_PROGRAM 18
#define PIN_LED_PROG 5

#define thermoDO 16
#define thermoCS 26
#define thermoCLK 25
#define BIN_BUZZER 4


#else

//#define PIN_ONLINE 18 // for green board
#define PIN_ONLINE 13 // for vero board
#define PIN_PROGRAM 33
#define PIN_LED_PROG 2

#define PIN_DOWNTIME_SWITCH 27
#define PIN_LED_FAULT 14
//#define PIN_INPUT4 33

#define PIN_INPUT1 34
#define PIN_INPUT2 35
#define PIN_INPUT3 PIN_DOWNTIME_SWITCH



#define thermoDO 16
#define thermoCS 26
#define thermoCLK 25
#define BIN_BUZZER 4


#endif

#endif /* PINSX_H_ */


