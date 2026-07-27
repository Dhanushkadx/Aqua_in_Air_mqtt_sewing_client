#include "aquasew_topics.h"

char AQ_TOPIC_INFO[AQ_TOPIC_LEN];
char AQ_TOPIC_TELE[AQ_TOPIC_LEN];
char AQ_TOPIC_RPC_RES[AQ_TOPIC_LEN];
char AQ_TOPIC_RPC_REQ[AQ_TOPIC_LEN];
char AQ_TOPIC_ATTR_SET[AQ_TOPIC_LEN];
char AQ_TOPIC_ATTR_RES[AQ_TOPIC_LEN];
char AQ_TOPIC_ATTR_REQUEST[AQ_TOPIC_LEN];
char AQ_TOPIC_ATTR_PUB[AQ_TOPIC_LEN];
char AQ_DEVICE_MAC[13];

void topics_init() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(AQ_DEVICE_MAC, sizeof(AQ_DEVICE_MAC),
        "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    snprintf(AQ_TOPIC_INFO,     sizeof(AQ_TOPIC_INFO),     AQ_TOPIC_ROOT "/%s/info",     AQ_DEVICE_MAC);
    snprintf(AQ_TOPIC_TELE,     sizeof(AQ_TOPIC_TELE),     AQ_TOPIC_ROOT "/%s/tele",     AQ_DEVICE_MAC);
    snprintf(AQ_TOPIC_RPC_RES,  sizeof(AQ_TOPIC_RPC_RES),  AQ_TOPIC_ROOT "/%s/rpc/res",  AQ_DEVICE_MAC);
    snprintf(AQ_TOPIC_RPC_REQ,  sizeof(AQ_TOPIC_RPC_REQ),  AQ_TOPIC_ROOT "/%s/rpc/req",  AQ_DEVICE_MAC);
    snprintf(AQ_TOPIC_ATTR_SET,     sizeof(AQ_TOPIC_ATTR_SET),     AQ_TOPIC_ROOT "/%s/attr/set",     AQ_DEVICE_MAC);
    snprintf(AQ_TOPIC_ATTR_RES,     sizeof(AQ_TOPIC_ATTR_RES),     AQ_TOPIC_ROOT "/%s/attr/res",     AQ_DEVICE_MAC);
    snprintf(AQ_TOPIC_ATTR_REQUEST, sizeof(AQ_TOPIC_ATTR_REQUEST), AQ_TOPIC_ROOT "/%s/attr/request", AQ_DEVICE_MAC);
    snprintf(AQ_TOPIC_ATTR_PUB,     sizeof(AQ_TOPIC_ATTR_PUB),     AQ_TOPIC_ROOT "/%s/attr/pub",     AQ_DEVICE_MAC);

    Serial.printf("Device MAC  : %s\n", AQ_DEVICE_MAC);
    Serial.printf("Topic INFO  : %s\n", AQ_TOPIC_INFO);
    Serial.printf("Topic TELE  : %s\n", AQ_TOPIC_TELE);
    Serial.printf("Topic RPC   : %s\n", AQ_TOPIC_RPC_REQ);
    Serial.printf("Topic ATTR  : %s\n", AQ_TOPIC_ATTR_SET);
    Serial.printf("Topic ARES  : %s\n", AQ_TOPIC_ATTR_RES);
    Serial.printf("Topic AREQ  : %s\n", AQ_TOPIC_ATTR_REQUEST);
    Serial.printf("Topic APUB  : %s\n", AQ_TOPIC_ATTR_PUB);
}
