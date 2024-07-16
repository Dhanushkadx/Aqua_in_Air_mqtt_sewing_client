// 
// 
// 

#include "input2_functions.h"



 // low CONT
 void fn_downTime_light_blink(){
	 
		 digitalWrite(PIN_LED_FAULT,HIGH);
         delay(100);
         digitalWrite(PIN_LED_FAULT,LOW);
         delay(100);

		
}


 //rising EDGE
 void fn_runDownTime_end_notify(int duration) {
	 curruntMCstate = IDLE;
     digitalWrite(PIN_LED_FAULT,LOW);
     data_updated_timeSeries = true;
 }

 //falling EDGE
 void fn_runDownTime_start_notify(int duration) {
	curruntMCstate = MC_FAULT;
    data_updated_timeSeries = true;
 }

