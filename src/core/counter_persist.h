#ifndef _COUNTER_PERSIST_H
#define _COUNTER_PERSIST_H

// Decides WHEN the production counters are written to flash. Two triggers:
//   1. Power-loss interrupt (boards with the super-cap + power-fail line): save
//      immediately within the ~200 ms hold-up window, so no counted piece is
//      lost on a mains drop. Guarded by POWER_LOSS_PIN — boards without the
//      hardware simply skip this path.
//   2. Periodic dirty-gate: every 60 s, save only if a counter actually changed
//      since the last write. Bounds worst-case loss on a crash/watchdog reboot
//      (which the power ISR cannot catch) to <= 60 s of production.
// Both run on Task2, the single writer of structSysData — snapshots are
// consistent and need no lock. The actual flash write is ConfigManager (NVS).

void counter_persist_init();   // seed the shadow + attach the power-fail ISR
void counter_persist_tick();   // call once per Task2 loop

#endif
