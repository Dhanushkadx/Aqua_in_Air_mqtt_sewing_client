// input0_functions.h

#ifndef _INPUT0_FUNCTIONS_h
#define _INPUT0_FUNCTIONS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#include "typex.h"
#include "TimerSW.h"
#include "statments.h"

extern uint8_t pulseCount;
// high CONT
void fn_production_idle_detect();

// Falling EDGE
void fn_productionCounter(int duration);

// rising EDGE
void fn_productionCounter_idle_detect_timer_reset(int duration);

#endif

