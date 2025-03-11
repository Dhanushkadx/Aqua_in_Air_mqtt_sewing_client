#include "Arduino.h"
#include "web_socket.h"
#include "input0_functions.h"
#include "input1_functions.h"
#include "input2_functions.h"
#include "tbBroker.h"
#include "clockConfig.h"

#include "config.h"
#include "web_server.h"
#include "ConfigManager.h"
// NEED ARDUINO JSON 6.18.0

#include "statments.h"
#include "esp_task_wdt.h"

#include "pinsx.h"

#include "sensor_scan.h"

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "TimerSW.h"
#include <ESPmDNS.h>
#include <Update.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#if defined(PLC_IOT_BRIDGE) || defined(IOT_PULSE_X)
#include "pixelx.h"
#endif

bool tbLoopDone = false;
bool SWrestart = false;
bool configMode_enable = false;

String lastDate;
// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;
Struct_GPIO_INFO GPIO_array[sensor_pin_count];
SYSCONFIG Struct_SYSCONFIG_INFO;
systemConfigTypedef_struct structSysConfig;
systemDataTypedef_struct structSysData;
char rtos_StringToSend[SIZE_StringToSend];
bool subscribed = false;
// LED number that is currently ON.
int current_led = 0;
char macStr[18];
char device_id_macStr[18];

TimerSW Timer_powerOnTimer;
TimerSW Timer_runTimer;
TimerSW Timer_acNoice;
TimerSW Timer_idle_detect;



//create handle for the mutex. It will be used to reference mutex
SemaphoreHandle_t  xMutex_dataTB;
//#define DEMO_MODE
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))


// Initialize underlying client, used to establish a connection
WiFiClient espClient;


TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
TaskHandle_t Task5;
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Task1code: TB com loop
void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());
  //reconnect();
  esp_task_wdt_init(10, true);
  while(1){
	   vTaskDelay(10 / portTICK_RATE_MS);
	  if (configMode_enable == false)
	  {
		   com_loop();   
	  }
  //
  } 
}

//Task2code: sensor scan loop
void Task2code( void * pvParameters ){
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());
  static uint32_t x=0;
  static uint32_t interval=15,prev_millis=0;
  
  for(;;){

   #ifdef DEMO_MODE // demo code
   uint32_t now = millis();   
   if(now-prev_millis>interval*1000){
    interval = random(20,35);
    prev_millis = now;
    xSemaphoreTake(xMutex, portMAX_DELAY);
    structSysData.productionCounter++;
    structSysData.TproductionCounter++;
    runTime = interval;
    data_updated_timeSeries = true;
    xSemaphoreGive(xMutex); // release mutex   
    }
   #endif
   
   fn_power_on();// count power on time
   sensor_scan();// scan inputs    
   
  vTaskDelay(10 / portTICK_RATE_MS);
  }
}

//Task3code: Time NTP
void Task3code( void * pvParameters ){
  Serial.print("Task3 running on core ");
  Serial.println(xPortGetCoreID());
  static uint32_t x=0;
  for(;;){
  
      //if (wifiStarted) {
         // ntp_loop();
     //   }
    vTaskDelay(10 / portTICK_RATE_MS);
  }
}


void Task5code(void* pvParameters){
  while(1){
    vTaskDelay(1 / portTICK_RATE_MS);
    if(ledState){
      digitalWrite(PIN_ONLINE,HIGH);
				delay(100);
				digitalWrite(PIN_ONLINE,LOW);
				delay(100);
    }
    else{

    }
  }
}

// Task4code: web server
void Task4code(void* pvParameters) {
    Serial.print("Task4 running on core ");
    Serial.println(xPortGetCoreID());
    static uint32_t x = 0;
    for (;;) {
        
        vTaskDelay(1 / portTICK_RATE_MS);
		if (configMode_enable)
		{
			digitalWrite(PIN_LED_PROG,HIGH);
		
		}
		else{
			if (wifiStarted == true)
			{
				digitalWrite(PIN_LED_PROG,HIGH);
				delay(100);
				digitalWrite(PIN_LED_PROG,LOW);
				delay(2500);
			}
			else{
				digitalWrite(PIN_LED_PROG,HIGH);
				delay(300);
				digitalWrite(PIN_LED_PROG,LOW);
				delay(300);
			}
		}
    }
}
void setup() {
	delay(2000);
#if defined(PLC_IOT_BRIDGE) || defined(IOT_PULSE_X)
 initPixelBright();
#endif
	 Serial.begin(SERIAL_DEBUG_BAUD);
	 Serial.println(WiFi.macAddress());
  
 
  Timer_powerOnTimer.interval = 1000;
  Timer_runTimer.interval = 1000;  
  Timer_acNoice.interval = 200;
  Timer_idle_detect.interval = 10000;
  Timer_idle_detect.previousMillis = millis();

  
  pinMode(PIN_ONLINE,OUTPUT);
  digitalWrite(PIN_ONLINE,LOW);
  pinMode(PIN_PROGRAM,INPUT_PULLUP);
  pinMode(PIN_LED_PROG,OUTPUT);
  pinMode(PIN_LED_FAULT,OUTPUT);

  
	initSPIFFS();	
 // ConfigManager::writeDefaultSystemConfig();
//	ConfigManager::writeDefaultSystemData();
  if (!digitalRead(PIN_PROGRAM))
  {
	 //ConfigManager :: writeDefaultSystemConfig();
	 ConfigManager :: loadSystemConfig(structSysConfig);
	 initWiFi_AP();
	 configMode_enable = true;
  }
  else{
	  configMode_enable = false;
	  ConfigManager::loadSystemConfig(structSysConfig);
	  initWiFi_STA();
  }
  uint8_t mac[6];
  WiFi.macAddress(mac);
	sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
// Format the MAC address without colons and with underscores
  sprintf(device_id_macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  ConfigManager :: loadSystemData(structSysData);
  initWebServerTimers();
	initTimers();  
  // create mutex and assign it a already create handler 
  xMutex_dataTB = xSemaphoreCreateMutex();
  initWebServices();

 GPIO_array[0].GPIOpin = PIN_INPUT1;
 GPIO_array[1].GPIOpin = PIN_INPUT2;
 GPIO_array[2].GPIOpin = PIN_INPUT3;
// GPIO_array[3].GPIOpin = PIN_INPUT4;
 
 // Production Count
 GPIO_array[0].fn_FALL_EDGE = fn_productionCounter;
 GPIO_array[0].fn_LOW_CONTINU = NULL;
 GPIO_array[0].fn_HIGH_CONTINU = fn_production_idle_detect;
 GPIO_array[0].fn_RISE_EDGE = fn_productionCounter_idle_detect_timer_reset;
 // Run Timer
 GPIO_array[1].fn_FALL_EDGE = NULL; //fn_runTime_count_reset;
 GPIO_array[1].fn_LOW_CONTINU = NULL;//fn_runTime_counter;
 GPIO_array[1].fn_HIGH_CONTINU = NULL;//fn_runTime_idle_detect;
 GPIO_array[1].fn_RISE_EDGE = NULL;//fn_runTime_idle_detect_timer_reset;
 // Fault detect
 GPIO_array[2].fn_FALL_EDGE = fn_runDownTime_start_notify;
 GPIO_array[2].fn_LOW_CONTINU = fn_downTime_light_blink;
 GPIO_array[2].fn_HIGH_CONTINU = NULL;
 GPIO_array[2].fn_RISE_EDGE = fn_runDownTime_end_notify;
  

  for(uint8_t index=0; index<sensor_pin_count; index++){
     pinMode(GPIO_array[index].GPIOpin,INPUT_PULLUP);
    }
 
  
  
  xTaskCreatePinnedToCore(
             Task1code,  /* Task function. */
             "Task1",    /* name of task. */
             20000,      /* Stack size of task */
             NULL,       /* parameter of the task */
             10,          /* priority of the task */
             &Task1,     /* Task handle to keep track of created task */
             0);
             
//create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */

 //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Task3code,   /* Task function. */
                    "Task3",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task3,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */


//create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
xTaskCreatePinnedToCore(
    Task4code,   /* Task function. */
    "Task4",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task4,      /* Task handle to keep track of created task */
    0);          /* pin task to core 1 */


//create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
xTaskCreatePinnedToCore(
    Task5code,   /* Task function. */
    "Task5",     /* name of task. */
    712,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task5,      /* Task handle to keep track of created task */
    0);          /* pin task to core 1 */
}



void loop(){
    vTaskDelay(10 / portTICK_RATE_MS);
	 cleanClients();
#if defined(PLC_IOT_BRIDGE) || defined(IOT_PULSE_X)
    if(configMode_enable){
      pixel_configEn();
    }
    else{
        if(wifiStarted){
            if(tbConnected){
              rainbow(5);
            }
            else{
                pixel_server_unreacheble();
            }
      
   }
   else{
         pixel_noWifi();
   }
    }
   
#endif 
}



  
  
 
