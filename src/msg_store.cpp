#include "msg_store.h"

// Path to store messages
#define FILE_PATH "/offline_msgs.json"

// Function to save message
void saveMessageToSPIFFS(const String &msg) {
    // Initialize SPIFFS if not already initialized
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }

    // Create or append to the file
    File file = SPIFFS.open(FILE_PATH, FILE_READ);
    String jsonString = "[]"; // Default empty array

    if (file) {
        jsonString = file.readString(); // Read existing messages
        file.close();
    }

    DynamicJsonDocument doc(4096); // Adjust size as needed
    deserializeJson(doc, jsonString);

    // Add a new message with a timestamp
    JsonArray array = doc["msg"];
    JsonObject newMsg = array.createNestedObject();
    newMsg["timestamp"] = millis(); // Replace with actual time if RTC is used
    newMsg["message"] = msg; // Set the message

    // Save updated array back to SPIFFS
    file = SPIFFS.open(FILE_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    serializeJson(doc, file);
    file.close();

    Serial.println("Message saved to SPIFFS");
}

// Function to process saved messages
bool processOfflineMessages() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return true;
    }

    File file = SPIFFS.open(FILE_PATH, FILE_READ);
    if (!file) {
        Serial.println("No offline messages found");
        return true;
    }

    String jsonString = file.readString();
    file.close();

    DynamicJsonDocument doc(5096); // Adjust size as needed
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.println("Failed to parse saved messages");
        return true;
    }

    JsonArray stored_msgs = doc["msg"];
      
 Serial.println("Processe offline messages");
    //JsonArray array = doc.as<JsonArray>();
    // Traditional indexed for loop
    for (JsonArray::iterator it=stored_msgs.begin(); it!=stored_msgs.end(); ++it) {
    JsonObject msg = (*it).as<JsonObject>();
    // Serialize JsonObject to String
        
    String serializedMsg;
    serializeJson(msg, serializedMsg);
    Serial.print("Stored msg>>>");
    Serial.println(serializedMsg);
        // Send each message over the network
        if (!sendNetworkMessage(serializedMsg)) {  // Assumes sendNetworkMessage() function is available
            stored_msgs.remove(it);
        } else {
           break; // Stop if network is unstable
        }
    }

    // Save updated messages back to SPIFFS
    file = SPIFFS.open(FILE_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return true;
    }
    serializeJson(doc, file);
    file.close();

   return false;
}

bool sendNetworkMessage(const String &msg){
    // Publish the JSON payload to the MQTT topic
				char mqttTopic [50];
				strcpy(mqttTopic,"nodered/sewing/");
				strcat(mqttTopic,device_id_macStr);

    Serial.printf_P(PSTR("send stored msg:%s"),msg.c_str());
    if ( client.publish(mqttTopic, msg.c_str())) {
        Serial.println(F(" successfully"));
        return false;
    } else {
        Serial.println(F(" Failed"));
        return true;
    } 
}
