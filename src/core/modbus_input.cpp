#include "modbus_input.h"
#include "../pinsx.h"

#ifdef HAS_MODBUS
#include <ModbusMaster.h>
#include "wifi_mqtt.h"          // mqtt_tx_enqueue
#include "mqtt_transport.h"     // MQTT_TX_PAYLOAD_MAX
#include "aquasew_topics.h"     // AQ_TOPIC_RPC_RES
#include "../statments.h"       // structSysData, structSysConfig, curruntMCstate
#include "../sewing_context.h"  // sewing_context_session_seq()

#define MODBUS_BAUD     19200
#define REG_BASE        32600        // pcs / run time / power-on, 2 regs each, LSW first
#define REG_COUNT       6
#define POLL_MS         5000
#define MB_MAX_READ     64           // ModbusMaster response buffer cap

static ModbusMaster   node;
static ModbusSnapshot s_snap;
static portMUX_TYPE   s_mux = portMUX_INITIALIZER_UNLOCKED;

// Generic read/write request queue — the ONLY way onto the bus besides the poll,
// so all RS485 traffic stays on the poll task (one owner).
enum { MB_OP_READ = 0, MB_OP_WRITE = 1 };
typedef struct { uint8_t op; uint16_t address; uint16_t length; uint16_t value; int reqId; } MbReq;
static QueueHandle_t s_reqQ = nullptr;

static void preTx()  { digitalWrite(MODBUS_DIR_PIN, HIGH); }   // RS485 transmit
static void postTx() { digitalWrite(MODBUS_DIR_PIN, LOW);  }   // RS485 receive

// Periodic read of the machine counters. CORRECTED map (backend's live-PLC read,
// change_request_modbus_register_map_correction): the PLC is LSW-FIRST — reg[even]
// is the low word, reg[odd] the high word — and the pairs are:
//   32600-01 = production count, 32602-03 = RUN TIME, 32604-05 = POWER-ON.
// (C cast to uint32_t before the shift, so it's a clean unsigned 32-bit assemble.)
static void poll_counters() {
    uint8_t r = node.readHoldingRegisters(REG_BASE, REG_COUNT);
    if (r == node.ku8MBSuccess) {
        uint32_t pcs = ((uint32_t)node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
        uint32_t run = ((uint32_t)node.getResponseBuffer(3) << 16) | node.getResponseBuffer(2);
        uint32_t pwr = ((uint32_t)node.getResponseBuffer(5) << 16) | node.getResponseBuffer(4);
        portENTER_CRITICAL(&s_mux);
        s_snap.pcs = pcs; s_snap.runtime = run; s_snap.poweron = pwr;
        s_snap.ok = true; s_snap.lastOkMs = millis();
        portEXIT_CRITICAL(&s_mux);
    } else {
        portENTER_CRITICAL(&s_mux); s_snap.ok = false; portEXIT_CRITICAL(&s_mux);
        Serial.printf("modbus: counter read failed (0x%02X)\n", r);
    }
}

// TODO(temps): read 3 temperatures from the 1000-block (2 regs each, 32-bit int,
// big-endian, scale TBD) into s_snap.temp[0..2], then add temp1/2/3 to the
// telemetry frame in sewing_tele.cpp. Addresses within 1000-1016 to be confirmed.

// Generic register read — publishes the rpc/res itself (runs on the poll task).
static void do_rpc_read(const MbReq& q) {
    char res[MQTT_TX_PAYLOAD_MAX];
    uint16_t len = q.length; if (len < 1) len = 1; if (len > MB_MAX_READ) len = MB_MAX_READ;
    uint8_t r = node.readHoldingRegisters(q.address, len);
    if (r == node.ku8MBSuccess) {
        int n = snprintf(res, sizeof(res),
            "{\"reqId\":\"%d\",\"success\":true,\"address\":%u,\"values\":[", q.reqId, q.address);
        for (uint16_t i = 0; i < len && n < (int)sizeof(res) - 12; i++)
            n += snprintf(res + n, sizeof(res) - n, i ? ",%u" : "%u", node.getResponseBuffer(i));
        snprintf(res + n, sizeof(res) - n, "]}");
    } else {
        snprintf(res, sizeof(res),
            "{\"reqId\":\"%d\",\"success\":false,\"error\":\"mb_0x%02X\"}", q.reqId, r);
    }
    mqtt_tx_enqueue(AQ_TOPIC_RPC_RES, res, false);
}

// Single holding-register write — publishes the rpc/res itself.
static void do_rpc_write(const MbReq& q) {
    char res[96];
    uint8_t r = node.writeSingleRegister(q.address, q.value);
    bool ok = (r == node.ku8MBSuccess);
    if (!ok) Serial.printf("modbus: write reg %u failed (0x%02X)\n", q.address, r);
    snprintf(res, sizeof(res),
        "{\"reqId\":\"%d\",\"success\":%s,\"address\":%u,\"value\":%u}",
        q.reqId, ok ? "true" : "false", q.address, q.value);
    mqtt_tx_enqueue(AQ_TOPIC_RPC_RES, res, false);
}

static void modbus_task(void*) {
    uint32_t lastPoll = 0;
    for (;;) {
        // 1. service any RPC read/write requests first — one bus, one owner.
        MbReq q;
        while (s_reqQ && xQueueReceive(s_reqQ, &q, 0) == pdTRUE) {
            if (q.op == MB_OP_READ) do_rpc_read(q);
            else                    do_rpc_write(q);
        }
        // 2. periodic machine poll — only while Modbus is the selected source.
        if (structSysConfig.input_mode == 1 &&
            (uint32_t)(millis() - lastPoll) >= POLL_MS) {
            lastPoll = millis();
            poll_counters();
            // poll_temps();  // TODO — see above
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void modbus_input_init() {
    s_reqQ = xQueueCreate(4, sizeof(MbReq));
    pinMode(MODBUS_DIR_PIN, OUTPUT);
    digitalWrite(MODBUS_DIR_PIN, LOW);
    Serial1.begin(MODBUS_BAUD, SERIAL_8E1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    Serial1.setTimeout(1000);
    node.begin(1, Serial1);                 // slave id 1
    node.preTransmission(preTx);
    node.postTransmission(postTx);
    xTaskCreatePinnedToCore(modbus_task, "modbus", 4096, NULL, 1, NULL, 1);
    Serial.println(F("modbus_input: started (RS485 19200 8E1, slave 1)"));
}

bool modbus_input_get(ModbusSnapshot* out) {
    portENTER_CRITICAL(&s_mux);
    *out = s_snap;
    portEXIT_CRITICAL(&s_mux);
    return out->ok;
}

void modbus_input_apply() {
    if (structSysConfig.input_mode != 1) return;   // GPIO mode: nothing to do

    ModbusSnapshot s;
    if (!modbus_input_get(&s)) return;             // no valid PLC read yet

    // Mirror the PLC's absolute counters. The PLC persists these itself, so on
    // our reboot we just re-read (counter_persist skips saving in Modbus mode).
    structSysData.productionCounter = s.pcs;
    structSysData.powerTime         = s.poweron;
    structSysData.runTime           = s.runtime;

    // count_total = pieces since the current manifest opened. Rebaseline on the
    // first read and whenever a new manifest opens (session-seq bump from R1).
    static bool     baseInit = false;
    static uint32_t baseline = 0;
    static uint32_t lastSeq  = 0;
    uint32_t seq = sewing_context_session_seq();
    if (!baseInit || seq != lastSeq) { baseline = s.pcs; lastSeq = seq; baseInit = true; }
    structSysData.count_total = (s.pcs >= baseline) ? (s.pcs - baseline) : 0;

    // runtime_state is NOT derived here — it is computed centrally in
    // runtime_state_update() (sensor_scan.cpp) from productionCounter advancing,
    // which we have just set from the PLC. Same rule for GPIO and Modbus modes.
}

bool modbus_rpc_read(int reqId, uint16_t address, uint16_t length) {
    if (!s_reqQ) return false;
    MbReq q = { MB_OP_READ, address, length, 0, reqId };
    return xQueueSend(s_reqQ, &q, 0) == pdTRUE;
}

bool modbus_rpc_write(int reqId, uint16_t address, uint16_t value) {
    if (!s_reqQ) return false;
    MbReq q = { MB_OP_WRITE, address, 0, value, reqId };
    return xQueueSend(s_reqQ, &q, 0) == pdTRUE;
}

#else  // !HAS_MODBUS — stubs so the firmware links on boards without RS485

void modbus_input_init() {}
bool modbus_input_get(ModbusSnapshot*) { return false; }
void modbus_input_apply() {}
bool modbus_rpc_read(int, uint16_t, uint16_t) { return false; }
bool modbus_rpc_write(int, uint16_t, uint16_t) { return false; }

#endif // HAS_MODBUS
