#ifndef _INPUT2_FUNCTIONS_h
#define _INPUT2_FUNCTIONS_h
#include "arduino.h"
#include "typex.h"
#include "TimerSW.h"
#include "statments.h"
#include "pinsx.h"

// low CONT
 void fn_downTime_light_blink();
 //rising EDGE
 void fn_runDownTime_end_notify(int duration);
 //falling EDGE
 void fn_runDownTime_start_notify(int duration);
#endif

