// input1_functions.h

#ifndef _INPUT1_FUNCTIONS_h
#define _INPUT1_FUNCTIONS_h
#include "arduino.h"
#include "typex.h"
#include "TimerSW.h"
#include "statments.h"

//void fn_set_falty_alarm(int dura);
void fn_runTime_counter();
void fn_runTime_idle_detect_timer_reset(int duration);
void fn_runTime_count_reset(int duration);
void fn_runTime_idle_detect();
#endif

