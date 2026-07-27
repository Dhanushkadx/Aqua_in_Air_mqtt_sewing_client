// mqtt_transport.h — shared MQTT queue structs and constants.
// Included by both gprs_mqtt.h (GSM build) and wifi_mqtt.h (WLAN build).
// When this architecture is extracted into a reusable library, this file
// becomes the library's public header — all products include only this.
#ifndef _MQTT_TRANSPORT_H
#define _MQTT_TRANSPORT_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ── Buffer size constants ──────────────────────────────────────────────────────
// TOPIC_MAX  : longest topic aquasew/<12-char MAC>/attr/pub = ~34 chars — 64 is safe
// TX_PAYLOAD : outbound — telemetry, RPC response, cfgIndex pub
// RX_PAYLOAD : inbound  — schedule block (largest expected payload) — 1200 bytes
#define MQTT_TOPIC_MAX        64
// 768: the PrimeFlow telemetry/event frame carries a 7-id context envelope on
// top of the counters, which pushes a worst-case frame past the old 512.
#define MQTT_TX_PAYLOAD_MAX   768
#define MQTT_RX_PAYLOAD_MAX   3072   // full cfgIndex version map at up to 64 blocks (~2.5 KB)
#define MQTT_TX_QUEUE_DEPTH   16   // 16 outbound slots — covers burst publish during reconnect
#define MQTT_RX_QUEUE_DEPTH   4    // 4 inbound slots  — callback is fast so rarely fills

// ── Outbound publish request ───────────────────────────────────────────────────
// Queued by application code (publish_tele, publish_info, cfgIndex pub, etc.).
// Drained by drain_mqtt_tx_queue() which runs inside com_loop() after client.loop().
// This guarantees client.publish() is never called from inside the MQTT callback.
struct MqttTxMsg {
    char topic[MQTT_TOPIC_MAX];
    char payload[MQTT_TX_PAYLOAD_MAX];
    bool retained;
};

// ── Inbound received message ───────────────────────────────────────────────────
// callback() copies the raw topic+payload here and enqueues it — zero business logic.
// drain_mqtt_inbound_queue() dequeues and routes to handle_rpc / handle_attr_res /
// handle_attr_set after client.loop() returns, so processing is safe and non-blocking.
struct MqttInboundMsg {
    char topic[MQTT_TOPIC_MAX];
    char payload[MQTT_RX_PAYLOAD_MAX];
};

// ── Queue handles — defined in the compiled transport module ───────────────────
// Declared extern so any module that includes this header can call mqtt_tx_enqueue().
extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_mqtt_inbound;

#endif // _MQTT_TRANSPORT_H
