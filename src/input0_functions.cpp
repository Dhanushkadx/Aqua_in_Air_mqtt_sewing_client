

#include "input0_functions.h"

uint8_t pulseCount = 0;
// high CONT
void fn_production_idle_detect() {
	
	if (Timer_idle_detect.Timer_run()) {
		data_updated_timeSeries = true;
		if(curruntMCstate != MC_FAULT){
			curruntMCstate = IDLE;
		}
	}
}


// Falling EDGE
void fn_productionCounter(int duration){
	//preScale 
	pulseCount++;
	Serial.print(F("pulseCount:"));
	Serial.print(pulseCount);
	if(pulseCount >= structSysConfig.preScale){
		pulseCount = 0;
		
		structSysData.productionCounter++;
		structSysData.TproductionCounter++;
		structSysData.DproductionCounter++;
		data_updated_timeSeries=true;
		Timer_idle_detect.previousMillis = millis();
		Serial.print(F("  ProductionCount:"));
		Serial.print(structSysData.productionCounter);
		Serial.print(F("  DotalProductionCount:"));
		Serial.println(structSysData.DproductionCounter);
		if (curruntMCstate != MC_FAULT)
		{
			/* code */curruntMCstate = MC_BUSY;
		}
		
	}
	
	
}

// rising EDGE
void fn_productionCounter_idle_detect_timer_reset(int duration){
	Timer_idle_detect.previousMillis=millis();
};
