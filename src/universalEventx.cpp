#include "universalEventx.h"

uint32_t prev_prdcount = 0;

byte universal_event_hadler(const char* smsbuffer){//0- gsm 1-lcd
	byte ret_value = 0;
	Serial.print("RX>");
	//delay(10000);
	Serial.print(smsbuffer);
	Serial.println("<<<<");
	//return ret_value;
	if (strncmp("count=",smsbuffer,6)==0)
	{
		char* eq_start = strstr(smsbuffer,"=");
		char count_char[50];		
		strcpy(count_char,eq_start+1);
		uint32_t count_int = atoi(count_char);
		Serial.printf_P(PSTR("Count:%d \n"),count_int);
		uint32_t actual_count = count_int - prev_prdcount;
		prev_prdcount = count_int;
		xSemaphoreTake(xMutex_dataTB, portMAX_DELAY);
		structSysData.DproductionCounter = actual_count + structSysData.DproductionCounter;
		xSemaphoreGive(xMutex_dataTB); // release mutex	
		data_updated_timeSeries=true;
       
	}

	else if (strncmp("length=",smsbuffer,7)==0)
	{
		char* eq_start = strstr(smsbuffer,"=");
		char length_char[50];		
		strcpy(length_char,eq_start+1);
		int length_int = atoi(length_char);
		Serial.printf_P(PSTR("Length:%d \n"),length_int);
		xSemaphoreTake(xMutex_dataTB, portMAX_DELAY);
		structSysData.length = length_int;
		xSemaphoreGive(xMutex_dataTB); // release mutex		
        
	}

	else if (strncmp("Start",smsbuffer,5)==0)
	{
		curruntMCstate=MC_BUSY;		
		data_updated_timeSeries = true;
		prev_prdcount = 0;
		Serial.println(F("MC BUSY"));
        
	}

	else if (strncmp("err",smsbuffer,3)==0)
	{
		curruntMCstate=MC_FAULT;
		data_updated_timeSeries = true;
		Serial.println(F("MC FAULT"));
		
	}

	else if (strncmp("Done",smsbuffer,4)==0)
	{
		curruntMCstate=IDLE;
		data_updated_timeSeries = true;
		Serial.println(F("MC IDLE"));		
		
	}

	
	return ret_value;
}