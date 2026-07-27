#ifndef _WIFI_MQTT_H
#define _WIFI_MQTT_H

#ifdef WLAN

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../pinsx.h"
#include "aquasew_topics.h"
#include "mqtt_transport.h"   // MqttTxMsg, MqttInboundMsg, queue handles

extern PubSubClient client;

// MQTT link state for the status LED. loopTask reads this cached flag instead
// of calling client.connected() itself — see the RTOS rule in wifi_mqtt.cpp.
extern volatile bool g_mqtt_online;

// Initialise queues, configure MQTT client, register WiFi events. Call once in setup().
void mqtt_setup();

// Called continuously from Task1 — connects MQTT when WiFi is up,
// pumps client.loop(), drains queues, ticks cfgIndex fetch state machine.
void com_loop();

// Enqueue a hw_caps client-attribute publish (model, input count, fw version,
// friendly name, location). Sent once per MQTT connect.
void publish_info();

// Telemetry lives in sewing_tele.cpp, not here — Task2 owns the counters and
// publishes them through mqtt_tx_enqueue(). This header stays transport-only.

// Safe publish from any task or context — enqueues to xQueue_mqtt_tx.
bool mqtt_tx_enqueue(const char* topic, const char* payload, bool retained = false);

// Convenience wrapper — calls mqtt_tx_enqueue internally.
bool send_mqtt_message(const char* topic, const char* message, bool retained = false);

#endif // WLAN
#endif // _WIFI_MQTT_H
