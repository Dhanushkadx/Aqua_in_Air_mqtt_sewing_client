#ifndef _MODBUS_INPUT_H
#define _MODBUS_INPUT_H

#include <Arduino.h>

// ── Modbus input source: mirror the machine PLC's counters over RS485 ─────────
// The SUPREM heat-transfer machine's PLC keeps its own production / power / run
// counters. In Modbus input mode we read them over RS485 (Modbus RTU) and feed
// them into the same telemetry pipeline the GPIO path uses, so no opto sensors
// are needed. All RS485 access is serialised on ONE poll task (it owns the bus);
// the generic read/write RPCs post requests to it rather than touching the bus.
//
// Everything here is a no-op on boards without HAS_MODBUS (no RS485 hardware).

struct ModbusSnapshot {
    uint32_t pcs;        // production count  (holding reg 32600-01)
    uint32_t poweron;    // power-on time     (holding reg 32602-03)
    uint32_t runtime;    // run time          (holding reg 32604-05)
    int32_t  temp[3];    // reserved — 3 temps from the 1000-block (not read yet)
    bool     ok;         // last periodic read succeeded
    uint32_t lastOkMs;   // millis() of the last good read
};

// Set up RS485 + start the poll task. Call once in setup(). No-op without HAS_MODBUS.
void modbus_input_init();

// Copy the latest snapshot (Task2). Returns the ok flag. false without HAS_MODBUS.
bool modbus_input_get(ModbusSnapshot* out);

// Mirror the PLC snapshot into the domain counters + machine state. Task2 only;
// does nothing unless input_mode == Modbus. Preserves the single-writer rule
// (Task2 is the sole writer of structSysData / curruntMCstate).
void modbus_input_apply();

// RPC bridge (called from the MQTT task): queue a generic holding-register read,
// or a single-register write. The poll task performs it on the bus and publishes
// the rpc/res itself. Returns false if the request queue is full/unavailable.
bool modbus_rpc_read(int reqId, uint16_t address, uint16_t length);
bool modbus_rpc_write(int reqId, uint16_t address, uint16_t value);

#endif // _MODBUS_INPUT_H
