

#include "sensor_scan.h"



void sensor_scan(){
	//Serial.println("pin change");
	for(uint8_t scan_index=0; scan_index<sensor_pin_count; scan_index++){
		
		 while(!(xSemaphoreTake( xMutex_dataTB, portMAX_DELAY )));
		 
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
		
		xSemaphoreGive(xMutex_dataTB);
	}
}


void fn_power_on(){
	static long previousMillis;
	unsigned long currentMillis = millis();
	
	if (currentMillis - previousMillis >= 1000) {
		// save the last time you blinked the LED
		previousMillis = currentMillis;
		//data_updated_timeSeries=true;
		 xSemaphoreTake(xMutex_dataTB, portMAX_DELAY);
		structSysData.TpowerTime++;
		structSysData.DpowerTime++;
		structSysData.powerTime++;
		 xSemaphoreGive(xMutex_dataTB); // release mutex
		//Serial.print("powerTime:");
		//Serial.println(structSysData.powerTime);
	}
	
}

 /// @brief 
 /// @param dura 
 void fn_reset_falty_alarm(int dura) {
	 data_updated_timeSeries = true;
	 Prev_faulty_alarm_status = false;
 }

 