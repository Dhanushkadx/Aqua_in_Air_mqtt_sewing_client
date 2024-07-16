// 
// 
// 

#include "input1_functions.h"


 
 /*void fn_set_falty_alarm(int dura){
	 data_updated_timeSeries = true;
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
		 structSysData.TrunTime++;
		 structSysData.DrunTime++;
		 structSysData.runTime++;
		 data_updated_timeSeries = true;
		 Serial.print(F("runTime"));
		 Serial.print(structSysData.runTime);
		 Serial.print(F(" TotalrunTime"));
		 Serial.println(structSysData.TrunTime);
	 }
	 
 }

 //rising EDGE
 void fn_runTime_idle_detect_timer_reset(int duration) {
	 actRun = false;
 }

 //falling EDGE
 void fn_runTime_count_reset(int duration) {
	 structSysData.runTime = 0;
	 actRun = true;
 }

 //high CONT
 void fn_runTime_idle_detect(){
	 if ((Timer_idle_detect.Timer_run()&&(curruntMCstate != MC_FAULT))) {
		 curruntMCstate = IDLE;
		 data_updated_timeSeries = true;
	 }
 }
