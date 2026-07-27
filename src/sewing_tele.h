#ifndef _SEWING_TELE_H
#define _SEWING_TELE_H

// ── Outbound: sewing production telemetry ─────────────────────────────────────
//
// The device's domain telemetry, deliberately OUTSIDE core/wifi_mqtt.cpp so the
// transport stays product-agnostic and merges cleanly with future src_core drops.
// It only ever reaches the broker through mqtt_tx_enqueue(), which copies by
// value into the FreeRTOS TX queue — the one thread-safe boundary.
//
// SINGLE-WRITER RULE (this is why there is no mutex anywhere near structSysData):
// every counter and machine-state flag is written ONLY by Task2 (the input-scan
// task) and read for publishing ONLY by Task2. Nothing crosses tasks except the
// finished JSON. That makes the snapshot consistent for free — nothing can
// modify a counter between the first field and the last — and removes any
// question about torn or stale cross-task reads.
//
// Commands that MUTATE the state come the other way, through sewing_cmd.h.
//
// Publish triggers (never per production pulse — the counters are monotonic, so
// a periodic snapshot already subsumes every increment since the last one):
//   - the updates_interval timer
//   - a machine-state transition (idle <-> busy <-> fault), which is rare
//   - an explicit request via sewing_tele_request_publish()

#include <Arduino.h>

// Prime the publish timer and the state-transition baseline. Call once in
// setup(), after config is loaded.
void sewing_tele_init();

// Ask for a frame on the next tick. Safe from Task2 only (sewing_cmd calls it);
// other tasks should post SEW_CMD_PUBLISH_NOW instead.
void sewing_tele_request_publish();

// Publish if requested / on a state change / on the timer. Task2 only.
void sewing_tele_tick();

#endif // _SEWING_TELE_H
