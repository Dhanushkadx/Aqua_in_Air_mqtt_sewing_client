#ifndef _SEWING_CMD_H
#define _SEWING_CMD_H

// ── Inbound commands: other tasks → Task2 (the sewing state owner) ────────────
//
// Task2 owns every production counter and machine-state flag: the input handlers
// write them, sewing_tele builds frames from them. Nothing else may write them.
//
// That matters because the counters are read-modify-written (`counter++`). A
// write from another task can land between that read and its write, so the
// increment puts the old value back and the other task's write is silently lost.
// An operator presses "reset" and the count keeps climbing.
//
// So any task needing to mutate this state posts a command here and Task2
// applies it between its own updates. Ordering falls out for free: a reset and a
// save issued from two different tasks are applied in the order they were sent,
// which independent flags could not guarantee (a save could otherwise persist
// pre-reset counters after the reset landed, and restore them on reboot).
//
// Transport for this is core/domain_cmd.h, which is product-agnostic; only the
// ids and the apply logic below are specific to this device.

#include <Arduino.h>

// There are no counter-reset / set-power / set-runtime commands: the session
// count (count_total) resets via the manifest lifecycle (R1), and the lifetime
// odometers are meant never to reset — ThingsBoard windows them for daily/shift.
// The one survivor is set_counter for the production odometer, kept purely as a
// device-replacement tool (migrate an accumulated count onto new hardware).
// `arg` carries the new value for SET_PRODUCTION and is ignored otherwise.
enum {
    SEW_CMD_SET_PRODUCTION = 1,  // arg = new productionCounter (device-swap only)
    SEW_CMD_PUBLISH_NOW,         // force a telemetry frame on the next tick
    SEW_CMD_SAVE_AND_RESTART,    // persist counters to flash, then reboot
    SEW_CMD_SET_INPUT_MODE,      // arg = input_mode (0=GPIO, 1=Modbus); persists config
};

// Drain and apply everything pending. Call from Task2 only — draining from two
// tasks would defeat the single-writer guarantee this exists to provide.
void sewing_cmd_tick();

#endif // _SEWING_CMD_H
