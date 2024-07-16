
#include "clockConfig.h"


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 16200;
const int   daylightOffset_sec = 3600;

void initRTC() {
  
   configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}


void printLocalTime()
{
	  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

}


void getDate(int *date_of_month){
	  struct tm timeinfo;
    
  if (!getLocalTime(&timeinfo,5000)) {
   // Serial.println("Failed to obtain time");
   timeinfo.tm_mday = structSysConfig.yesterDay;
  }
  else{
    *date_of_month = timeinfo.tm_mday;
  }
 
}

