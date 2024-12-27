#include "msg_store.h"


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

// Function to save message
void saveMessageToSPIFFSV2(JsonDocument& msg) {
    // Initialize SPIFFS if not already initialized
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }

    // Open the file for reading
    File file = SPIFFS.open(FILE_PATH, FILE_READ);
    String jsonString = "[]"; // Default empty array in case the file is empty

    if (file) {
        jsonString = file.readString(); // Read existing messages from the file
        file.close();
    }

    DynamicJsonDocument doc(4096); // Adjust size as needed
    deserializeJson(doc, jsonString);

    // Create or get the "msg" array
    JsonArray array = doc["msg"]; 
    if (!array) {
        // If "msg" array doesn't exist, create one
        array = doc.createNestedArray("msg");
    }

    // Add the received message (`msg`) as a new object inside the "msg" array
    JsonObject newMsg = array.createNestedObject();
    
    // Copy all fields from the passed `msg` (JsonDocument) into the new object
    for (JsonPair keyValue : msg.as<JsonObject>()) {
        newMsg[keyValue.key()] = keyValue.value(); // Copy each key-value pair
    }

    // Save the updated JSON document back to SPIFFS
    file = SPIFFS.open(FILE_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    serializeJson(doc, file);  // Write the updated JSON to the file
    file.close();

    Serial.println("Message saved to SPIFFS");
}

// Function to save message
void saveMessageToSPIFFSV3(JsonDocument& msg) {
    uint32_t file_size;
    // Initialize SPIFFS if not already initialized
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }

    // Get the next available file index
    int fileIndex = getNextFileIndex();
    String currentFileName = FILE_PREFIX + String(fileIndex) + FILE_EXTENSION;

    // Check if the current file exists and its size
    File file = SPIFFS.open(currentFileName, FILE_READ);
    String jsonString = "[]"; // Default empty array in case the file is empty
    if (file) {
        jsonString = file.readString();  // Read existing messages from the file
        file_size = file.size();
        file.close();
    }

    DynamicJsonDocument doc(4096); // Adjust size as needed
    deserializeJson(doc, jsonString);

    // Create or get the "msg" array
    JsonArray array = doc["msg"];
    if (!array) {
        // If "msg" array doesn't exist, create one
        array = doc.createNestedArray("msg");
    }

    // Add the received message (`msg`) as a new object inside the "msg" array
    JsonObject newMsg = array.createNestedObject();

    // Copy all fields from the passed `msg` (JsonDocument) into the new object
    for (JsonPair keyValue : msg.as<JsonObject>()) {
        newMsg[keyValue.key()] = keyValue.value(); // Copy each key-value pair
    }

    // Save the updated JSON document back to SPIFFS (in the new file if necessary)
    file = SPIFFS.open(currentFileName, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    
    serializeJson(doc, file);  // Write the updated JSON to the file
    file.close();

    // Check if the current file size exceeds 4KB
    if (file_size > MAX_FILE_SIZE) {
        // If file size exceeds 4KB, increment the file index and create a new file
        fileIndex++;
        Serial.println("go to next file");
        saveFileIndex(fileIndex);  // Save the new file index
        //currentFileName = FILE_PREFIX + String(fileIndex) + FILE_EXTENSION;
    }

    Serial.println("Message saved to SPIFFS: " + currentFileName);
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
        Serial.println(F("ok"));
        return false;
    } else {
        Serial.println(F("Failed"));
        return true;
    } 
    
}

// Function to get the next available file index
uint8_t getNextFileIndex() {
    int fileIndex = 1;
    File file = SPIFFS.open(FILE_INDEX_PATH, FILE_READ);
    
    if (file) {
        String indexStr = file.readString();
        file.close();
        if (indexStr.length() > 0) {
            fileIndex = indexStr.toInt();  // Read the last file index
        }
    }
    
    return fileIndex;
}

// Function to save the updated file index to SPIFFS
void saveFileIndex(int fileIndex) {
    File file = SPIFFS.open(FILE_INDEX_PATH, FILE_WRITE);
    
    if (file) {
        file.print(fileIndex);  // Save the new file index
        file.close();
    }
}

// Function to process offline messages from multiple files
bool processOfflineMessagesV2() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return true;
    }

    int fileIndex = 1; // Start from messages_1.json
    bool processed_result = true;

    while (true) {
        String currentFileName = FILE_PREFIX + String(fileIndex) + FILE_EXTENSION;
        // Check if the file exists
        if (!SPIFFS.exists(currentFileName)) {
            Serial.println("No more files to process.");
            // After processing all files, reset fileIndex to 1 and save it back
            saveFileIndex(1);  // Save the new file index
            processed_result = false;
            break;  // No more files, exit the loop
        }

        File file = SPIFFS.open(currentFileName, FILE_READ);
        if (!file) {
            Serial.println("Failed to open file for reading");
            break;
        }
          // Check if the file is empty
        if (file.size() == 0) {
            Serial.println("File is empty, skipping...");
            file.close();  // Close the file
            fileIndex++;   // Skip to the next file
            continue;      // Skip processing the current empty file
        }

        String jsonString = file.readString();
        Serial.printf_P(PSTR("MSG_HDLE-FILE %d CONT: %s \n"),fileIndex,jsonString.c_str());
       
        file.close();  // Close the file after reading

        DynamicJsonDocument doc(4096); // Adjust size as needed
        DeserializationError error = deserializeJson(doc, jsonString);
        if (error) {
            Serial.println("Failed to parse saved messages");
            continue;  // Skip this file and move to the next one
        }

        JsonArray stored_msgs = doc["msg"];

        Serial.println("Processing offline messages");
        // Iterate through each message and try to send it
        for (JsonArray::iterator it = stored_msgs.begin(); it != stored_msgs.end(); ++it) {
            JsonObject msg = (*it).as<JsonObject>();
            String serializedMsg;
            serializeJson(msg, serializedMsg);

            Serial.printf_P(PSTR("MSG_HDLE-NOW SEND: %s \n"),serializedMsg.c_str());
            Serial.println(serializedMsg);

            // Send each message over the network
            if (!sendNetworkMessage(serializedMsg)) {  // Assumes sendNetworkMessage() function is available
                // If sending ok, remove the message from the array
                stored_msgs.remove(it);
            } else {
                Serial.println(F("MSG_HDLE- Sending Faild Processing Faild!!")); 
                return true;
                processed_result = true;
                break; // Stop if network is unstable
            }
        }

        // Save updated messages back to the current file
        file = SPIFFS.open(currentFileName, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open file for writing");
            return true;
        }
        serializeJson(doc, file);
        file.close();

        // Move to the next file if there are more files to process
        fileIndex++;
        
    }

    return processed_result;
}
