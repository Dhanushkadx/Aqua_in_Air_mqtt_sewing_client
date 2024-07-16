/* 
* TimerSW.h
*
* Created: 4/21/2021 11:00:25 AM
* Author: dhanu
*/


#ifndef __TIMERSW_H__
#define __TIMERSW_H__
#include "Arduino.h"

class TimerSW
{
//variables
public:
protected:
private:

//functions
public:
	TimerSW();
	~TimerSW();
	boolean Timer_run();
	long previousMillis;
	long interval;
protected:
private:
	TimerSW( const TimerSW &c );
	TimerSW& operator=( const TimerSW &c );

}; //TimerSW

#endif //__TIMERSW_H__
