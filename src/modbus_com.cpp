#include "modbus_com.h"

// voltage, current and frequency register of Energy Meter
// if you want to read more parameters, then increase the array size
//and, write the address
uint16_t data_register[3] = {0x0000, 0x0008, 0x0036};
uint16_t prevValues[3] = {0}; // Array to store previous register values
//Initialize the ModbusMaster object as node
ModbusMaster node;

// Pin 4 made high for Modbus transmision mode
void modbusPreTransmission()
{
  delay(5);
  digitalWrite(MODBUS_DIR_PIN, HIGH);
}

// Pin 4 made low for Modbus receive mode
void modbusPostTransmission()
{
  digitalWrite(MODBUS_DIR_PIN, LOW);
  delay(5);
}

void setupModbus() {
  //we initialize the built-in hardware serial communication 
  //using the Serial.begin() function with two parameters.
  //The first parameter is the desired baud rate (9600 bits per second), 
  //and the second parameter is SERIAL_8E1, 
  //which specifies the data format (8 data bits, even parity, and 1 stop bit).
  //to set these two parameter, please read your sensor datasheet first
  Serial.begin(115200);
  
   Serial2.begin(9600, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
  Serial2.setTimeout(1000);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial1.print("Hi");

  pinMode(MODBUS_DIR_PIN, OUTPUT);
  digitalWrite(MODBUS_DIR_PIN, LOW);

  //modbus device slave ID 14
  node.begin(0x01, Serial2);

//  callbacks allow us to configure the RS485 transceiver correctly
   node.preTransmission(modbusPreTransmission);
   node.postTransmission(modbusPostTransmission);
}

void Modbusloop()
{

uint16_t data_buffer[3] = {0}; // Buffer to store new register values

  // Example: Read registers starting from address 0x00 and store in data_buffer
  if (ModbusRead(0x00, data_buffer)) {
    Serial.println("Register values have changed!");

    // Print the new register values
    for (int i = 0; i < 3; i++) {
      Serial.print("New Value at Register ");
      Serial.print(0x00 + i);
      Serial.print(": ");
      Serial.println(data_buffer[i]);
    }
  } else {
    //Serial.println("No changes detected in the registers.");
  }

  delay(1000);  // One-second delay before checking again
}


bool ModbusRead(uint16_t startAddress, uint16_t newValues[3]) {
  uint8_t result;
 
  bool hasChanged = false;

  // Read 3 consecutive registers starting from the given address
  result = node.readHoldingRegisters(startAddress, 1);
  if (result == node.ku8MBSuccess) {
    Serial.println("Success, Received data: ");
    for (int i = 0; i < 3; i++) {
      newValues[i] = node.getResponseBuffer(i);

      // Check if the current value is different from the previous value
      if (newValues[i] != prevValues[i]) {
        hasChanged = true;
        prevValues[i] = newValues[i];
      }

      
    }
  } else {
    Serial.print("Failed, Response Code: ");
    Serial.print(result, HEX);
    Serial.println("");
  }

  // Return true if any register value has changed
  return hasChanged;
}


