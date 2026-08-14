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

// Central running/idle derivation, used in BOTH input modes: running while the
// production counter keeps advancing, idle after RUN_IDLE_TIMEOUT_MS with no new
// piece, unless the GPIO fault switch has latched MC_FAULT. Deliberately NOT
// level-based on a GPIO — idle level differs per machine (some pull the line low
// when idle, some high), so a raw-level read gives false results. Task2 only.
void runtime_state_update();

void fn_power_on();

void fn_reset_falty_alarm(int dura);


#endif /* SENSOR_SCAN_H_ */