#include "domain_cmd.h"

static QueueHandle_t s_queue = nullptr;

void domain_cmd_init() {
    s_queue = xQueueCreate(DOMAIN_CMD_QUEUE_DEPTH, sizeof(DomainCmd));
    if (!s_queue) Serial.println(F("domain_cmd: queue creation failed"));
}

bool domain_cmd_post(uint16_t cmd, int32_t arg) {
    if (!s_queue) return false;
    DomainCmd c = { cmd, arg };
    // Timeout 0 — callers include the MQTT task and the system event task,
    // neither of which may block on the domain task. A full queue means the
    // owner has stalled; dropping is the right failure here (the operator can
    // retry) and it is logged.
    bool ok = (xQueueSendToBack(s_queue, &c, 0) == pdPASS);
    if (!ok) Serial.printf("domain_cmd: queue full — cmd %u dropped\n", cmd);
    return ok;
}

bool domain_cmd_take(DomainCmd* out) {
    if (!s_queue || !out) return false;
    return (xQueueReceive(s_queue, out, 0) == pdPASS);
}
