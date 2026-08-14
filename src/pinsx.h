/*
 * pinsx.h
 *
 * Created: 12/13/2022 8:37:35 AM
 *  Author: DhanushkaC
 */ 

#ifndef PINSX_H_
#define PINSX_H_

// Define your configurations based on the use case
//#define PLC_IOT_BRIDGE  // Comment or Uncomment based on your setup
#define IOT_PULSE_X  // Uncomment for IOT Pulse X setup
//#define VERO_BOARD

#ifdef IOT_PULSE_X
    // Specific pin mappings for IOT_PULSE_X
    #define RELAY1 13
    #define RELAY2 27
    #define RELAY3 18
    #define RELAY4 2
    #define RELAY5 19
    #define RELAY6 22
    #define PIN_PROGRAM PIN_INPUT4
    #define PIN_LED_WIFI RELAY2
    #define PIN_DOWNTIME_SWITCH 36
    #define PIN_LED_FAULT RELAY1
    #define PIN_ONLINE RELAY3
    #define PIN_INPUT1 35  // 19
    #define PIN_INPUT2 34  // 22
    #define PIN_INPUT3 PIN_DOWNTIME_SWITCH
    #define PIN_INPUT4 39
    #define PIN_RS485_FLCTRL 5
    #define POWER_LOSS_PIN 21     // super-cap power-fail line: idles LOW, driven HIGH on power drop (RISING)
    // RS485 Modbus RTU to the machine PLC (SUPREM). HAS_MODBUS gates the whole
    // modbus_input module + the Modbus input-source path; boards without it (VERO)
    // never compile the RS485 task, so GPIO25/26 stay free for their thermo pins.
    #define HAS_MODBUS
    #define MODBUS_RX_PIN  25
    #define MODBUS_TX_PIN  26
    #define MODBUS_DIR_PIN PIN_RS485_FLCTRL   // RS485 DR/RE direction (GPIO5)
    #define thermoDO 12
    #define thermoCS 15
    #define thermoCLK 14
    #define BIN_BUZZER 4
    // Define the LED pin and the number of LEDs in the WS2812B strip
    #define RGB_LED_PIN 23        // GPIO pin where WS2812B is connected
    #define NUM_LEDS 1       // Only one LED


#elif defined(PLC_IOT_BRIDGE)
    // Specific pin mappings for PLC_IOT_BRIDGE
    #define RELAY1 32
    #define RELAY2 33
    #define PIN_DOWNTIME_SWITCH 23
    #define PIN_LED_FAULT RELAY1
    #define PIN_ONLINE RELAY2
    #define PIN_INPUT1 22  // 19
    #define PIN_INPUT2 21  // 22
    #define PIN_INPUT3 PIN_DOWNTIME_SWITCH
    #define PIN_PROGRAM 18
    #define PIN_LED_WIFI 5
    #define thermoDO 16
    #define thermoCS 26
    #define thermoCLK 25
    #define BIN_BUZZER 4
    #define RGB_LED_PIN 13        // GPIO pin where WS2812B is connected
    #define NUM_LEDS 1       // Only one LED

#elif defined(VERO_BOARD)
    // Default pin mappings (for the vero board)
    #define PIN_ONLINE 13  // For vero board
    #define PIN_PROGRAM 33
    #define PIN_LED_WIFI 2
    #define PIN_DOWNTIME_SWITCH 27
    #define PIN_LED_FAULT 14
    // #define PIN_INPUT4 33 // Unused or commented for now
    #define PIN_INPUT1 34
    #define PIN_INPUT2 35
    #define PIN_INPUT3 PIN_DOWNTIME_SWITCH
    #define thermoDO 16
    #define thermoCS 26
    #define thermoCLK 25
    #define BIN_BUZZER 4
    // Vero board has no WS2812 status pixel (WiFi state blinks on PIN_LED_WIFI,
    // MQTT/cloud status on PIN_ONLINE).
    // Stub the pixel macros so led_driver compiles as a no-op — 0 LEDs means its
    // loops never run and nothing is driven. Matches the legacy pixelx behavior.
    #define RGB_LED_PIN 0
    #define NUM_LEDS 0

#endif

#endif /* PINSX_H_ */
