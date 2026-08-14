#ifndef _SEWING_CONTEXT_H
#define _SEWING_CONTEXT_H

// ── PrimeFlow context: machine binding + active manifest (the job) ────────────
//
// Consumes the two cfgIndex blocks the backend pushes — "machine_config" and
// "active_manifest" (doc/11C sec 6.1) — and holds the resulting context that
// tags every telemetry frame and event (the 7-id envelope, doc/11C sec 5.1).
//
// TASK OWNERSHIP. A block is fetched on the MQTT task (Task1). But applying a
// manifest change RESETS the session counter (count_total), which is a Task2
// counter — writing it from Task1 would race the input handler's increment
// (the single-writer rule, sewing_cmd.h). So the fetched block is handed to
// Task2 through a queue, and ALL application happens on Task2:
//     Task1 cfgindex apply handler  ->  enqueue raw block
//     Task2 sewing_context_tick()   ->  parse + apply (R1-R4), reset counter,
//                                        emit events, persist context
// The context state is therefore written AND read only by Task2 — no lock.
//
// Manifest lifecycle (doc/11C sec 5, "Critical Rules"):
//   R1 manifest_id changed        -> new job: reset count_total, adopt session,
//                                     emit manifest_loaded, resume counting
//   R2 same id, higher version    -> update style/target only, NO reset
//   R3 status == "closed"         -> stop counting, emit manifest_closed
//   R4 no manifest / id null      -> idle; behave as standalone (counting on)

#include <Arduino.h>

// Create the block queue, register the cfgIndex apply handler, load persisted
// context. Call once in setup(), after config_init() (LittleFS) and before Task2
// starts. cfgindex_set_apply_handler is wired here.
void sewing_context_init();

// Drain queued blocks and apply them (R1-R4). Task2 only.
void sewing_context_tick();

// True while the session counter (count_total) should keep incrementing.
// False only while a manifest is explicitly closed (R3). The input handler
// checks this before bumping count_total. Same-task read (Task2).
bool sewing_context_counting_enabled();

// Monotonic session sequence — bumped each time a NEW manifest opens (R1). The
// Modbus input path watches this to rebaseline count_total (pcs - baseline at
// manifest open), since in Modbus mode the counter comes from the PLC, not the
// per-piece increment sewing_context resets. Task2 read.
uint32_t sewing_context_session_seq();

// Append the 7-id context envelope as JSON fields — no braces, no leading or
// trailing comma: "business_id":..,"plant_id":..,..,"operator_id":null
// Unset ids are emitted as null (Q3). Used by both telemetry and events so the
// envelope is identical on every message. Returns chars written.
int sewing_context_append_ids(char* buf, size_t len);

// Emit an event on the telemetry topic with the full envelope + event_type + an
// optional payload object (pass nullptr or "{}" for none). Task2 only — it
// enqueues to the MQTT TX queue. ts is stamped from NTP epoch when the clock is
// synced, otherwise omitted (the bridge stamps on arrival).
void sewing_event_emit(const char* event_type, const char* payload_json);

#endif // _SEWING_CONTEXT_H
