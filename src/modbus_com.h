#ifndef MODBUSCOM_H
#define MODBUSCOM_H


#include "Arduino.h"
#include <ModbusMaster.h>

/* Modbus Configuration */
#define MODBUS_DIR_PIN  5    // Pin for MAX485 DR, RE
#define MODBUS_RX_PIN 25     // Rx pin
#define MODBUS_TX_PIN 26     // Tx pin
#define MODBUS_SERIAL_BAUD 9600 // Baud rate for Modbus RTU communication



// Initialize the ModbusMaster object
extern ModbusMaster node;

/**
 * Pre-transmission setup for Modbus (RS485 transmit mode)
 * Sets the direction pin to HIGH.
 */
void modbusPreTransmission();

/**
 * Post-transmission setup for Modbus (RS485 receive mode)
 * Sets the direction pin to LOW.
 */
void modbusPostTransmission();

/**
 * Setup function to initialize Modbus and Serial communication.
 */
void setupModbus();

void Modbusloop();

bool ModbusRead(uint16_t startAddress, uint16_t newValues[3] = nullptr);

#endif