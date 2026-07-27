// 
// 
// 

#include "input1_functions.h"


 
 /*void fn_set_falty_alarm(int dura){
	 Prev_faulty_alarm_status = true;
	 Serial.println("fault....");
 }*/
 
 // low CONT
 void fn_runTime_counter(){
	 if (Timer_runTimer.Timer_run()){
		 if ((structSysData.runTime>30)&&(curruntMCstate != MC_FAULT))
		 {
			 curruntMCstate = MC_BUSY;
		 }		 
		 // Cumulative now, not a per-run stopwatch — it is one of the three
		 // absolute counters ThingsBoard windows. NOTE: none of the
		 // GPIO_array[1] callbacks are registered in main.cpp today, so this
		 // never runs and runTime stays 0 until input 1 is wired up.
		 structSysData.runTime++;
		 Serial.print(F("runTime"));
		 Serial.println(structSysData.runTime);
	 }

 }

 //rising EDGE
 void fn_runTime_idle_detect_timer_reset(int duration) {
	 actRun = false;
 }

 //falling EDGE
 void fn_runTime_count_reset(int duration) {
	 // No longer zeroes runTime — it is an absolute lifetime counter now and
	 // only an explicit RPC may reset it. This just marks the machine running.
	 actRun = true;
 }

 //high CONT
 void fn_runTime_idle_detect(){
	 if ((Timer_idle_detect.Timer_run()&&(curruntMCstate != MC_FAULT))) {
		 curruntMCstate = IDLE;
	 }
 }
