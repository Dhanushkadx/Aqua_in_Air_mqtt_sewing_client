#include "sewing_cmd.h"
#include "sewing_tele.h"
#include "statments.h"
#include "ConfigManager.h"
#include "core/domain_cmd.h"

// After any counter change: write it to flash immediately, then push a frame.
// Persisting straight away matters because these are the ONLY reset points —
// an operator reset that a power cut loses would silently come back, which is
// worse than not resetting at all. Publishing straight away means the operator
// sees the new value in the next frame instead of waiting out the interval.
static void persist_and_publish() {
    ConfigManager::saveSystemData(structSysData);
    sewing_tele_request_publish();
}

// Runs on Task2, which owns every field touched here.
static void apply_cmd(const DomainCmd& c) {
    switch (c.cmd) {
        // Device-replacement only: set the lifetime production odometer. A
        // negative arg would wrap when stored in an unsigned counter, so it is
        // rejected rather than silently turned into ~4 billion.
        case SEW_CMD_SET_PRODUCTION:
            if (c.arg < 0) { Serial.println(F("sewing_cmd: negative value rejected")); break; }
            structSysData.productionCounter = (unsigned int)c.arg;
            Serial.printf("sewing_cmd: productionCounter set to %ld\n", (long)c.arg);
            persist_and_publish();
            break;

        case SEW_CMD_PUBLISH_NOW:
            sewing_tele_request_publish();
            break;

        case SEW_CMD_SET_INPUT_MODE:
            // Production input source selector (0 = GPIO, 1 = Modbus). Applied on
            // Task2 and persisted here (config write off the MQTT task). The modbus
            // task + Task2's modbus_input_apply() pick up the new value next loop.
            structSysConfig.input_mode = (uint8_t)(c.arg ? 1 : 0);
            ConfigManager::saveSystemConfig(structSysConfig);
            Serial.printf("sewing_cmd: input_mode -> %u (persisted)\n", structSysConfig.input_mode);
            sewing_tele_request_publish();
            break;

        case SEW_CMD_SAVE_AND_RESTART:
            // Posted by the WiFi disconnect handler after its reconnect timeout.
            // Deliberately done HERE and not there: that handler runs on the
            // system event task, whose stack is ~2304 bytes by default, and
            // saveSystemData() puts a 1256-byte StaticJsonDocument on it — far
            // too tight. On Task2 (10000-byte stack) it is comfortable, and the
            // counters it serialises belong to this task, so the snapshot is
            // consistent instead of racing the increments.
            Serial.println(F("sewing_cmd: saving counters, then restarting"));
            ConfigManager::saveSystemData(structSysData);
            delay(100);   // let the flash write settle and the log flush
            ESP.restart();
            break;

        default:
            Serial.printf("sewing_cmd: unknown cmd %u\n", c.cmd);
            break;
    }
}

void sewing_cmd_tick() {
    DomainCmd c;
    while (domain_cmd_take(&c)) apply_cmd(c);
}
