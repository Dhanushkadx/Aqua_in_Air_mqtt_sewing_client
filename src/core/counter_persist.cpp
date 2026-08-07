#include "counter_persist.h"
#include "../ConfigManager.h"
#include "../statments.h"     // structSysData (extern)
#include "../pinsx.h"         // POWER_LOSS_PIN (board-dependent)
#include <Arduino.h>
#include <string.h>

static const uint32_t SAVE_INTERVAL_MS = 60000;   // periodic dirty-gate window

static uint32_t s_lastSaveMs = 0;
static systemDataTypedef_struct s_shadow;          // last values written to flash

#ifdef POWER_LOSS_PIN
// Set in the ISR only; actioned on Task2. The power-fail line idles LOW and is
// driven HIGH by the comparator when the rail starts to drop, so we trip on the
// RISING edge and save during the super-cap hold-up.
static volatile bool s_powerLost = false;
static void IRAM_ATTR onPowerLoss() { s_powerLost = true; }
#else
// Boards with no power-fail line (e.g. VERO) cannot catch a mains drop, so they
// checkpoint at every natural pause instead: this tracks the machine state to
// detect a running -> stopped transition (a finished production burst).
static eMC_state s_prevMC = UNK;
#endif

static void save_now(const char* why) {
	ConfigManager::saveSystemData(structSysData);
	s_shadow     = structSysData;
	s_lastSaveMs = millis();
	Serial.printf("counter_persist: saved (%s)\n", why);
}

void counter_persist_init() {
	s_shadow     = structSysData;   // seed from what loadSystemData put in RAM
	s_lastSaveMs = millis();
#ifdef POWER_LOSS_PIN
	pinMode(POWER_LOSS_PIN, INPUT_PULLDOWN);   // idle low; comparator drives high on drop
	attachInterrupt(digitalPinToInterrupt(POWER_LOSS_PIN), onPowerLoss, RISING);
#else
	s_prevMC = curruntMCstate;
#endif
}

void counter_persist_tick() {
#ifdef POWER_LOSS_PIN
	// Power-fail: save immediately, once. No deep-sleep — if the rail is truly
	// gone the chip dies right after the write (NVS/LittleFS writes are power-safe
	// so a half-finished write cannot corrupt the stored counters); if it was a
	// dip that recovers, we simply carry on with a fresh checkpoint on flash.
	if (s_powerLost) {
		s_powerLost = false;
		save_now("power_loss");
		return;
	}
#else
	// No power-fail hardware: checkpoint at each production pause. On a
	// running -> stopped edge the burst's final count is written, bounding a
	// reboot's loss to the current continuous run (zero if stopped at reboot).
	if (s_prevMC == MC_BUSY && curruntMCstate != MC_BUSY) {
		if (memcmp(&s_shadow, &structSysData, sizeof(s_shadow)) != 0)
			save_now("state_idle");
	}
	s_prevMC = curruntMCstate;
#endif
	// Periodic dirty-gate backstop: only write when something actually changed.
	if ((uint32_t)(millis() - s_lastSaveMs) >= SAVE_INTERVAL_MS) {
		if (memcmp(&s_shadow, &structSysData, sizeof(s_shadow)) != 0)
			save_now("periodic");
		else
			s_lastSaveMs = millis();   // nothing changed; re-arm the window
	}
}
