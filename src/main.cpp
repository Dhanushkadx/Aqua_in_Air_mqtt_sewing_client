#include "Arduino.h"
#include "web_socket.h"
#include "input0_functions.h"
#include "input1_functions.h"
#include "input2_functions.h"
#include "clockConfig.h"
#include <esp_wifi.h>
#include "Config.h"
#include "web_server.h"
#include "ConfigManager.h"

// PrimeHive connectivity core — MQTT transport, chunked OTA, cfgIndex, portal.
#include "core/wifi_mqtt.h"
#include "core/wifi_com.h"
#include "core/wifi_creds.h"
#include "core/wifi_portal.h"
#include "core/cfgindex.h"
#include "core/led_status.h"
#include "core/OTA.h"
#include "core/aquasew_topics.h"
#include "core/domain_cmd.h"
#include "core/counter_persist.h"
#include "core/modbus_input.h"
#include "sewing_cmd.h"
#include "sewing_tele.h"
#include "sewing_context.h"
// NEED ARDUINO JSON 6.18.0

#include "statments.h"
#include "esp_task_wdt.h"

#include "pinsx.h"

#include "sensor_scan.h"

#include <WiFi.h>
#include "TimerSW.h"
#include <ESPmDNS.h>
#include <Update.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

bool configMode_enable = false;

Struct_GPIO_INFO GPIO_array[sensor_pin_count];
systemConfigTypedef_struct structSysConfig;
systemDataTypedef_struct structSysData;
char macStr[18];
char device_id_macStr[18];

TimerSW Timer_powerOnTimer;
TimerSW Timer_runTimer;
TimerSW Timer_idle_detect;

// Sewing machine-state globals. These lived in tbBroker.cpp, which was the
// transport AND the domain state holder; the transport moved to core/wifi_mqtt
// (which must stay domain-agnostic), so the state lands here with the rest of
// the device globals.
//
// SINGLE-WRITER: Task2 is the only task that touches these — the input handlers
// write them, sewing_tele_tick() reads them to build the frame. No lock is
// needed and none exists. Do NOT write them from Task1 (the MQTT task); post a
// domain_cmd instead, or a read-modify-write can be silently lost.
bool Prev_faulty_alarm_status = false;
bool actRun = false;
eMC_state prevMCstate = UNK;      // edge detector for the transition publish
eMC_state curruntMCstate = IDLE;



//#define DEMO_MODE
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))


// Initialize underlying client, used to establish a connection
//WiFiClient espClient;


// Task1 = MQTT/OTA/cfgIndex (com_loop), Task2 = input scan. The old Task3 (an
// empty NTP stub — clockConfig owns the clock), Task4 and Task5 (PIN_LED_PROG /
// PIN_ONLINE blink patterns) are gone: the WS2812 status pixel driven by
// core/led_status from loop() now shows the whole connectivity state machine.
TaskHandle_t Task1;
TaskHandle_t Task2;

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

   #ifdef DEMO_MODE // demo code — synthesises production without real inputs
   // No lock: this runs on Task2, which owns these counters. All three are the
   // absolute monotonic counters, so runTime accumulates here rather than being
   // set to the interval.
   uint32_t now = millis();
   if(now-prev_millis>interval*1000){
    interval = random(20,35);
    prev_millis = now;
    structSysData.productionCounter++;
    structSysData.powerTime += interval;
    structSysData.runTime   += interval;
    }
   #endif
   
   // Inbound first, so a reset / manifest change lands before the frame that
   // reports it. Both drain queues fed by other tasks and apply on Task2.
   sewing_cmd_tick();       // RPC commands: resets, set_counter, save+restart
   sewing_context_tick();   // cfgIndex blocks: machine_config, active_manifest (R1-R4)

   fn_power_on();// count power on time
   sensor_scan();// scan inputs

   // Modbus input source: mirror the PLC's counters over the GPIO-derived ones.
   // No-op in GPIO mode / on boards without RS485.
   modbus_input_apply();

   // Derive running/idle from the production counter advancing — after BOTH the
   // GPIO scan and the Modbus mirror have set it, so one rule serves both modes.
   runtime_state_update();

   // Persist the counters: power-loss save (INT) + 60 s dirty-gated periodic.
   // After sensor_scan() so it sees this iteration's increments.
   counter_persist_tick();

   // Task2 owns every counter and machine-state flag, so it is also what
   // publishes them. Nothing crosses tasks except the finished JSON.
   sewing_tele_tick();  // outbound — build + enqueue telemetry + events

  vTaskDelay(10 / portTICK_RATE_MS);
  }
}

void setup() {
	delay(2000);
	 Serial.begin(SERIAL_DEBUG_BAUD);  
 
  Timer_powerOnTimer.interval = 1000;
  Timer_runTimer.interval = 1000;  
  Timer_idle_detect.interval = 10000;
  Timer_idle_detect.previousMillis = millis();

  
  pinMode(PIN_ONLINE,OUTPUT);
  digitalWrite(PIN_ONLINE,LOW);
  pinMode(PIN_PROGRAM,INPUT_PULLUP);
  pinMode(PIN_LED_WIFI,OUTPUT);
  pinMode(PIN_LED_FAULT,OUTPUT);

  
	config_init();   // mounts LittleFS (format on failure) + loads cfgIndex versions
	wifi_creds_init();  // /wifi.json, or the compile-time defaults on first boot

	// WiFi enrollment portal — PIN_PROGRAM held LOW for 3s, or the pending flag
	// set by the enter_portal RPC. NEVER RETURNS: it reboots on save or timeout.
	// Must run before any task is created, while nothing else is touching the radio.
	if (portal_should_enter()) portal_run();

	ota_boot_check();   // latch the NVS marker from a prior OTA (success/aborted)
	led_status_begin();

	// A SHORT press (not held long enough for the portal above) still enters the
	// legacy device-config AP, which serves the richer system_config.json editor
	// that the credential portal deliberately does not duplicate.
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
	  initWiFi_STA();       // core/wifi_com.cpp — reads creds from wifi_creds
  }
  uint8_t mac[6];
  WiFi.macAddress(mac);
	sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
// Format the MAC address without colons and with underscores
  sprintf(device_id_macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  ConfigManager :: loadSystemData(structSysData);
  counter_persist_init();   // seed save-shadow from loaded counters + arm power-fail ISR
  initWebServerTimers();
  // No data mutex any more: structSysData and the machine-state flags are
  // written AND read only by Task2 (single-writer), and the only thing crossing
  // tasks is the finished JSON via the MQTT TX queue. Commands travel the other
  // way through domain_cmd, so nothing needs a lock.
  domain_cmd_init();      // MQTT task -> Task2 command channel
  sewing_context_init();  // machine_config/active_manifest consumer + persisted context
  sewing_tele_init();     // publish timer + state-transition baseline
  initWebServices();

  // Transport last: topics_init() needs the MAC, and mqtt_setup() registers the
  // WiFi events + creates the TX/RX queues that Task1's com_loop() drains.
  topics_init();
  mqtt_setup();

  // RS485 Modbus poll task (Modbus input source + read/write RPC bus owner).
  // No-op on boards without HAS_MODBUS. After config load so input_mode is known.
  modbus_input_init();

 GPIO_array[0].GPIOpin = PIN_INPUT1;
 GPIO_array[1].GPIOpin = PIN_INPUT2;
 GPIO_array[2].GPIOpin = PIN_INPUT3;
// GPIO_array[3].GPIOpin = PIN_INPUT4;
 
 // Production Count
 GPIO_array[0].fn_FALL_EDGE = fn_productionCounter;
 GPIO_array[0].fn_LOW_CONTINU = NULL;
 GPIO_array[0].fn_HIGH_CONTINU = NULL;   // idle now from runtime_state_update(), not GPIO level
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

}



void loop(){
  vTaskDelay(10 / portTICK_RATE_MS);
	 cleanClients();
	 // The whole connectivity state machine (connecting / WiFi up / cloud up /
	 // OTA) is now one pixel driven by core/led_status. It replaces the pixelx
	 // block that used to live here — which was a no-op anyway, since pixelx.cpp
	 // locally redefines NUM_LEDS to 0 and RGB_LED_PIN to 0.
	 led_status_tick();   // renders the status pixel; reads cached flags only
}



  
  
 
