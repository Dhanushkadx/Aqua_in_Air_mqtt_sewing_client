#ifndef OFFLINEMESSAGEHANDLER_H
#define OFFLINEMESSAGEHANDLER_H

#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "tbBroker.h"
// Constants
#define FILE_PATH "/offline_msgs.json" // Path to store offline messages
#define JSON_DOC_SIZE 4096            // Adjust based on expected JSON message size
extern PubSubClient client;
// Function Declarations

/**
 * @brief Save a message to SPIFFS as part of a JSON array.
 * 
 * @param msg The JSON message string to be saved.
 */
void saveMessageToSPIFFS(const String &msg);

/**
 * @brief Process and send offline messages from SPIFFS if the network is available.
 */
bool processOfflineMessages();

/**
 * @brief Mock function to check network availability.
 * 
 * @return true if the network is available, false otherwise.
 */
bool isNetworkAvailable();

/**
 * @brief Send a message over the network.
 * 
 * @param msg The JSON message to be sent.
 * @return true if the message was sent successfully, false otherwise.
 */
bool sendNetworkMessage(const String &msg);

#endif // OFFLINEMESSAGEHANDLER_H
