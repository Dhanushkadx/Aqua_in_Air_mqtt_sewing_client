// web_socket.h

#ifndef _WEB_SOCKET_h
#define _WEB_SOCKET_h

#include "typex.h"
#include "config.h"
#include "web_socket.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "TimerSW.h"
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>

void notifyClients_pageIndex();

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len);
void onEvent(AsyncWebSocket *server,AsyncWebSocketClient *client,AwsEventType type,void *arg,uint8_t *data,size_t len);

#endif // WEBSOCKET_H

