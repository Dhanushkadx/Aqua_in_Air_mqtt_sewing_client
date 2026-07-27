#ifndef _DOMAIN_CMD_H
#define _DOMAIN_CMD_H

// ── Any task → the owning task: command channel ───────────────────────────────
//
// Enforces single-writer ownership of state that is read-modify-written. A task
// that does not own such state must not write it: a `counter_reset` zeroing a
// counter while the owner runs `counter++` can land between that read and its
// write, so the increment puts the old value back and the reset is silently
// lost. (Plain assignments to a mask are mostly benign; RMW is not.)
//
// So the non-owner posts a request and the owner applies it between its own
// updates. No lock, no lost update. Ordering is free too: commands from several
// tasks are applied in the order they were sent, which independent flags could
// not guarantee.
//
// Not tied to MQTT despite living in the transport folder — posters on this
// device are the MQTT task (RPC handlers in wifi_mqtt.cpp) AND the system event
// task (the WiFi reconnect-timeout save in wifi_com.cpp). Any task may post;
// exactly one task drains.
//
// Deliberately product-agnostic — `cmd`/`arg` are opaque here and defined by the
// product (see sewing_cmd.h for this device's ids), so this file can live in the
// shared core unchanged across products.
//
// Usage:
//   any task  : domain_cmd_post(MY_CMD_FOO, 0);        // never blocks
//   owner task: DomainCmd c;
//               while (domain_cmd_take(&c)) { ... }    // drain at top of loop

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// 8 slots — commands are operator-initiated and rare; the owner drains every
// loop, so this only has to absorb a burst arriving between two drains.
#define DOMAIN_CMD_QUEUE_DEPTH 8

struct DomainCmd {
    uint16_t cmd;   // product-defined command id
    int32_t  arg;   // product-defined argument (0 when unused)
};

// Create the queue. Call once in setup(), before any task that uses it starts.
void domain_cmd_init();

// Post a command from any task, including a WiFi/system event callback. Copies
// by value into the FreeRTOS queue and never blocks, so it is safe in contexts
// that must not stall. Returns false if the queue is full (dropped, logged).
bool domain_cmd_post(uint16_t cmd, int32_t arg = 0);

// Take the next pending command. Returns false when empty. Call from the owning
// task only — draining from two tasks would defeat the point.
bool domain_cmd_take(DomainCmd* out);

#endif // _DOMAIN_CMD_H
