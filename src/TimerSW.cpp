/* 
* TimerSW.cpp
*
* Created: 4/21/2021 11:00:25 AM
* Author: dhanu
*/


#include "TimerSW.h"

// default constructor
TimerSW::TimerSW()
{
} //TimerSW

// default destructor
TimerSW::~TimerSW()
{
} //~TimerSW


boolean TimerSW ::  Timer_run(){
	unsigned long currentMillis = millis();
	if(currentMillis - previousMillis > interval) {
		// save the last time you blinked the LED
		previousMillis = currentMillis;
		return true;

	}
	return false;
}