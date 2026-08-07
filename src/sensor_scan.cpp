

#include "sensor_scan.h"



void sensor_scan(){
	//Serial.println("pin change");
	// No lock: Task2 is the sole writer AND the sole reader of everything the
	// callbacks below touch (sewing_tele_tick() builds the frame on this same
	// task). The mutex that used to wrap this loop was also held across the
	// Serial.print calls inside fn_productionCounter, stalling the MQTT task for
	// milliseconds at a time — that problem goes away with it.
	for(uint8_t scan_index=0; scan_index<sensor_pin_count; scan_index++){

		GPIO_array[scan_index].nowState = digitalRead(GPIO_array[scan_index].GPIOpin);
		if(GPIO_array[scan_index].prevState!=GPIO_array[scan_index].nowState){
			long now = millis();
			int duration = now-GPIO_array[scan_index].last_changed_time;
			GPIO_array[scan_index].last_changed_time = now;
			GPIO_array[scan_index].prevState = GPIO_array[scan_index].nowState;
			if(!GPIO_array[scan_index].nowState){

				if (GPIO_array[scan_index].fn_FALL_EDGE != NULL) { GPIO_array[scan_index].fn_FALL_EDGE(duration);}
				
			}
			else{

				if(GPIO_array[scan_index].fn_RISE_EDGE!=NULL){GPIO_array[scan_index].fn_RISE_EDGE(duration);}
				
			}
		}

		if(!GPIO_array[scan_index].nowState){
			if(GPIO_array[scan_index].fn_LOW_CONTINU!=NULL){GPIO_array[scan_index].fn_LOW_CONTINU();}
		}
		else {
			if (GPIO_array[scan_index].fn_HIGH_CONTINU != NULL) { GPIO_array[scan_index].fn_HIGH_CONTINU();}
		}
	}
}


void fn_power_on(){
	static long previousMillis;
	unsigned long currentMillis = millis();
	
	if (currentMillis - previousMillis >= 1000) {
		// save the last time you blinked the LED
		previousMillis = currentMillis;
		// Same task as sensor_scan() and sewing_tele_tick() — single writer, no lock.
		structSysData.powerTime++;
		// runTime = cumulative SECONDS in the running state, ticked here in the
		// same 1 s block as powerTime. "running" is derived from production pulses
		// (input0 sets MC_BUSY), so this needs no separate run-signal opto and the
		// invariant runTime <= powerTime holds automatically. Matches the backend
		// contract (change_request_runtime_odometer.txt): monotonic, persisted,
		// increases only while runtime_state == "running".
		if (curruntMCstate == MC_BUSY) structSysData.runTime++;
		//Serial.print("powerTime:");
		//Serial.println(structSysData.powerTime);
	}
	
}

 /// @brief 
 /// @param dura 
 void fn_reset_falty_alarm(int dura) {
	 Prev_faulty_alarm_status = false;
 }

 