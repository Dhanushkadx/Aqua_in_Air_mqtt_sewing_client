/*
 * sensor_scan.h
 *
 * Created: 5/11/2023 9:12:42 AM
 *  Author: DhanushkaC
 */ 


#ifndef SENSOR_SCAN_H_
#define SENSOR_SCAN_H_

#include "statments.h"
#include "Arduino.h"
#include "typex.h"

extern Struct_GPIO_INFO GPIO_array[];

void sensor_scan();


void fn_power_on();

void fn_reset_falty_alarm(int dura);


#endif /* SENSOR_SCAN_H_ */