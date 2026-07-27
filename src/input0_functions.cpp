

#include "input0_functions.h"
#include "sewing_context.h"   // sewing_context_counting_enabled()

uint8_t pulseCount = 0;
// high CONT
void fn_production_idle_detect() {
	
	if (Timer_idle_detect.Timer_run()) {
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

		structSysData.productionCounter++;   // lifetime odometer, never stops
		// Session counter only advances while a manifest is open (R3 stops it on
		// close). With no manifest at all it counts too — standalone behaviour.
		if (sewing_context_counting_enabled()) structSysData.count_total++;
		Timer_idle_detect.previousMillis = millis();
		Serial.print(F("  ProductionCount:"));
		Serial.println(structSysData.productionCounter);
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
