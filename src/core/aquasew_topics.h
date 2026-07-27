#ifndef _AQUASEW_TOPICS_H
#define _AQUASEW_TOPICS_H

#include <Arduino.h>
#include <WiFi.h>
#include "../default_config.h"

// Root level of every topic this device speaks. MUST match what the backend
// Node-RED flows subscribe to (they wildcard <root>/+/...) and what the EMQX
// ACL grants the device credential (<root>/<MAC>/#). Single knob (its value
// lives in default_config.h) — OTA.cpp and wifi_mqtt.cpp build topics from this.
#define AQ_TOPIC_ROOT DEFAULT_TOPIC_ROOT

// Maximum topic string length
#define AQ_TOPIC_LEN 64

// Topic strings — populated once by topics_init()
extern char AQ_TOPIC_INFO[AQ_TOPIC_LEN];     // <root>/<MAC>/info       (publish: hw_caps)
extern char AQ_TOPIC_TELE[AQ_TOPIC_LEN];     // <root>/<MAC>/tele       (publish: telemetry)
extern char AQ_TOPIC_RPC_RES[AQ_TOPIC_LEN];  // <root>/<MAC>/rpc/res    (publish: RPC response)
extern char AQ_TOPIC_RPC_REQ[AQ_TOPIC_LEN];  // <root>/<MAC>/rpc/req    (subscribe: RPC commands)
extern char AQ_TOPIC_ATTR_SET[AQ_TOPIC_LEN];     // <root>/<MAC>/attr/set     (subscribe: cfgIndex pushes while connected)
extern char AQ_TOPIC_ATTR_RES[AQ_TOPIC_LEN];     // <root>/<MAC>/attr/res     (subscribe: server responses to device requests)
extern char AQ_TOPIC_ATTR_REQUEST[AQ_TOPIC_LEN]; // <root>/<MAC>/attr/request (publish: device requests a key by ID)
extern char AQ_TOPIC_ATTR_PUB[AQ_TOPIC_LEN];     // <root>/<MAC>/attr/pub     (publish: device reports its local cfgIndex versions)

// Device MAC (no colons, uppercase) — e.g. "AABBCCDDEEFF"
extern char AQ_DEVICE_MAC[13];

// Call once after WiFi.begin() — reads WiFi MAC and builds all topic strings
void topics_init();

#endif
