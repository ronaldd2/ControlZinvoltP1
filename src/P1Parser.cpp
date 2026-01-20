/*
 * P1Parser.cpp - Implementation of P1 Parser
 */

#include "P1Parser.h"

// OBIS codes for DSMR P1 telegrams
#define OBIS_TIMESTAMP "0-0:1.0.0"
#define OBIS_ENERGY_IMPORT_TARIFF1 "1-0:1.8.1"
#define OBIS_ENERGY_IMPORT_TARIFF2 "1-0:1.8.2"
#define OBIS_ENERGY_EXPORT_TARIFF1 "1-0:2.8.1"
#define OBIS_ENERGY_EXPORT_TARIFF2 "1-0:2.8.2"
#define OBIS_POWER_DELIVERED "1-0:1.7.0"
#define OBIS_POWER_RECEIVED "1-0:2.7.0"
#define OBIS_VOLTAGE_L1 "1-0:32.7.0"
#define OBIS_VOLTAGE_L2 "1-0:52.7.0"
#define OBIS_VOLTAGE_L3 "1-0:72.7.0"
#define OBIS_CURRENT_L1 "1-0:31.7.0"
#define OBIS_CURRENT_L2 "1-0:51.7.0"
#define OBIS_CURRENT_L3 "1-0:71.7.0"
#define OBIS_POWER_DELIVERED_L1 "1-0:21.7.0"
#define OBIS_POWER_DELIVERED_L2 "1-0:41.7.0"
#define OBIS_POWER_DELIVERED_L3 "1-0:61.7.0"
#define OBIS_POWER_RECEIVED_L1 "1-0:22.7.0"
#define OBIS_POWER_RECEIVED_L2 "1-0:42.7.0"
#define OBIS_POWER_RECEIVED_L3 "1-0:62.7.0"

P1Parser::P1Parser() {
  valid = false;
  activePowerL1 = 0;
  activePowerL2 = 0;
  activePowerL3 = 0;
  activePowerDeliveredL1 = 0;
  activePowerDeliveredL2 = 0;
  activePowerDeliveredL3 = 0;
  totalEnergyImport = 0;
  totalEnergyExport = 0;
  currentL1 = 0;
  currentL2 = 0;
  currentL3 = 0;
  voltageL1 = 0;
  voltageL2 = 0;
  voltageL3 = 0;
}

bool P1Parser::parse(const String& telegram) {
  rawTelegram = telegram;
  valid = false;
  
  // Check if telegram starts with '/' and ends with '!'
  if (!telegram.startsWith("/") || (telegram.charAt(telegram.length()-5) != '!')) {
    Serial.println("Invalid P1 telegram format");
    return false;
  }
  
  // Extract values using OBIS codes
  timestamp = extractObisValue(telegram, OBIS_TIMESTAMP);
  
  // Energy totals
  String energyImport1 = extractObisValue(telegram, OBIS_ENERGY_IMPORT_TARIFF1);
  String energyImport2 = extractObisValue(telegram, OBIS_ENERGY_IMPORT_TARIFF2);
  String energyExport1 = extractObisValue(telegram, OBIS_ENERGY_EXPORT_TARIFF1);
  String energyExport2 = extractObisValue(telegram, OBIS_ENERGY_EXPORT_TARIFF2);
  
  totalEnergyImport = energyImport1.toFloat() + energyImport2.toFloat();
  totalEnergyExport = energyExport1.toFloat() + energyExport2.toFloat();
  
  // Voltages
  voltageL1 = extractObisValue(telegram, OBIS_VOLTAGE_L1).toFloat();
  voltageL2 = extractObisValue(telegram, OBIS_VOLTAGE_L2).toFloat();
  voltageL3 = extractObisValue(telegram, OBIS_VOLTAGE_L3).toFloat();
  
  // Currents
  currentL1 = extractObisValue(telegram, OBIS_CURRENT_L1).toFloat();
  currentL2 = extractObisValue(telegram, OBIS_CURRENT_L2).toFloat();
  currentL3 = extractObisValue(telegram, OBIS_CURRENT_L3).toFloat();
  
  // Active power per phase (consumption - positive)
  activePowerL1 = extractObisValue(telegram, OBIS_POWER_DELIVERED_L1).toFloat();
  activePowerL2 = extractObisValue(telegram, OBIS_POWER_DELIVERED_L2).toFloat();
  activePowerL3 = extractObisValue(telegram, OBIS_POWER_DELIVERED_L3).toFloat();
  
  // Power delivered back (generation - negative for consumption calculation)
  activePowerDeliveredL1 = extractObisValue(telegram, OBIS_POWER_RECEIVED_L1).toFloat();
  activePowerDeliveredL2 = extractObisValue(telegram, OBIS_POWER_RECEIVED_L2).toFloat();
  activePowerDeliveredL3 = extractObisValue(telegram, OBIS_POWER_RECEIVED_L3).toFloat();
  
  // Calculate net power (positive = consuming, negative = generating)
  activePowerL1 -= activePowerDeliveredL1;
  activePowerL2 -= activePowerDeliveredL2;
  activePowerL3 -= activePowerDeliveredL3;
  
  valid = true;
  return true;
}

String P1Parser::extractObisValue(const String& telegram, const String& obisCode) {
  int startIndex = telegram.indexOf(obisCode);
  if (startIndex == -1) {
    return "";
  }
  
  // Find the opening parenthesis after OBIS code
  int openParen = telegram.indexOf('(', startIndex);
  if (openParen == -1) {
    return "";
  }
  
  // Find the closing parenthesis
  int closeParen = telegram.indexOf(')', openParen);
  if (closeParen == -1) {
    return "";
  }
  
  // Extract value between parentheses
  String value = telegram.substring(openParen + 1, closeParen);
  
  // Remove units (everything after *)
  int asteriskPos = value.indexOf('*');
  if (asteriskPos != -1) {
    value = value.substring(0, asteriskPos);
  }
  
  return value;
}

float P1Parser::extractValue(const String& line) {
  int startPos = line.indexOf('(');
  int endPos = line.indexOf('*');
  
  if (startPos == -1 || endPos == -1) {
    return 0.0;
  }
  
  String valueStr = line.substring(startPos + 1, endPos);
  return valueStr.toFloat();
}
// CRC16 calculation for P1 telegrams (CRC16-CCITT with polynomial 0xA001)
String P1Parser::calculateCRC16(const String& data) {
  if (data.length() == 0) {
    return "0000";
  }
  
  uint16_t crc = 0x0000;
  
  for (int pos = 0; pos < data.length(); pos++) {
    crc ^= (uint8_t)data[pos];
    for (int i = 0; i < 8; i++) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  
  // Return as 4-character hex string
  char hexStr[5];
  sprintf(hexStr, "%04X", crc);
  return String(hexStr);
}

// Validate CRC in received telegram
// Format: /...\ndata\n!CCCC\nwhere CCCC is the CRC
// CRC is calculated over data from '/' to '!' (NOT including the CRC itself)
bool P1Parser::validateCRC(const String& telegram) {
  if (telegram.length() < 6) {
    return false;  // Telegram too short (min: /x!CCCC)
  }
  
  // Find the last '!' which indicates end of data
  int exclamationPos = telegram.lastIndexOf('!');
  if (exclamationPos == -1 || exclamationPos < 1) {
    return false;  // No exclamation mark
  }
  
  // Extract CRC from telegram (should be 4 chars after '!')
  if (telegram.length() < exclamationPos + 5) {
    return false;  // Not enough characters for CRC
  }
  
  String receivedCRC = telegram.substring(exclamationPos + 1, exclamationPos + 5);
  
  if (receivedCRC.length() != 4) {
    return false;  // CRC should be 4 characters
  }
  
  // Calculate CRC for data from '/' to '!' (NOT including the CRC chars after '!')
  String dataForCRC = telegram.substring(0, exclamationPos + 1);  // Include the '!' in calculation
  String calculatedCRC = calculateCRC16(dataForCRC);
  
  // Compare CRCs (case-insensitive)
  return receivedCRC.equalsIgnoreCase(calculatedCRC);
}