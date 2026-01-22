/*
 * P1Modifier.cpp - Implementation of P1 Modifier
 */

#include "P1Modifier.h"

// OBIS codes for power values per phase
#define OBIS_POWER_DELIVERED_L1 "1-0:21.7.0"
#define OBIS_POWER_DELIVERED_L2 "1-0:41.7.0"
#define OBIS_POWER_DELIVERED_L3 "1-0:61.7.0"
#define OBIS_POWER_RECEIVED_L1 "1-0:22.7.0"
#define OBIS_POWER_RECEIVED_L2 "1-0:42.7.0"
#define OBIS_POWER_RECEIVED_L3 "1-0:62.7.0"
#define OBIS_POWER_DELIVERED "1-0:1.7.0"
#define OBIS_POWER_RECEIVED "1-0:2.7.0"

P1Modifier::P1Modifier() {
  currentMode = MODE_UNMODIFIED;
  batteryPhase = 1;
  modifyPhase = 1;
  forcePower = 2000.0;  // Default 2kW
  externalControlPower = 0.0;
  externalControlLastUpdate = 0;
}

String P1Modifier::getModeString() const {
  switch (currentMode) {
    case MODE_UNMODIFIED: return "Unmodified";
    case MODE_OFF: return "Off";
    case MODE_FORCE_CHARGE: return "Force Charge";
    case MODE_FORCE_DISCHARGE: return "Force Discharge";
    case MODE_CHARGE_ONLY: return "Charge Only";
    case MODE_DISCHARGE_ONLY: return "Discharge Only";
    case MODE_EXTERNAL_CONTROL: return "External Control";
    default: return "Unknown";
  }
}

String P1Modifier::modify(const String& originalTelegram, const P1Parser& parser, float batteryPower) {
  // If unmodified mode, return original
  if (currentMode == MODE_UNMODIFIED) {
    return originalTelegram;
  }
  String modifiedTelegram = originalTelegram;
  
  float activePowerWatt[4];
  float newPowerWatt[4] = {0, 0, 0, 0};
  for (int phase = 0; phase <=3; phase++) {
    activePowerWatt[phase] = parser.getActivePower(phase)*1000.0; // Convert kW to W  
  }

  // Add random noise (-1, 0, +1 W) to make battery see value changes
  if (++_noise > 1) _noise = -1; // Cycle through -1, 0, +1

  newPowerWatt[0] = (activePowerWatt[0]) + _noise; // Start with total power plus noise

  
  switch (currentMode) {
    case MODE_OFF:
      // Gradual power reduction - halve battery contribution
      newPowerWatt[0] -= (batteryPower / 4.0);
      break;
      
    case MODE_FORCE_CHARGE:
      // Show high export to force charging (house → grid)
      newPowerWatt[0] = -forcePower - (batteryPower / 4.0);  // Convert W to kW
      break;
      
    case MODE_FORCE_DISCHARGE:
      // Show high import to force discharging (grid → house)
      newPowerWatt[0] = forcePower + (batteryPower / 4.0);  // Convert W to kW
      break;
      
    case MODE_CHARGE_ONLY:
      // Only allow charging - suppress export
      if (batteryPower < 0) {
        newPowerWatt[0] -= (batteryPower / 4.0); // Reduce export
      }
      break;
      
    case MODE_DISCHARGE_ONLY:
      // Only allow discharging - suppress import
      if (batteryPower > 0) {
        newPowerWatt[0] -= (batteryPower / 4.0); // Reduce import
      }
      break;
      
    case MODE_EXTERNAL_CONTROL:
      // External control via REST API
      if (isExternalControlValid()) {
        // External control is valid (received within 60 seconds)
        newPowerWatt[0] = externalControlPower;
      }
      break;     
    default:
      break;
  }
  // Distribute new power across phases
  if (modifyPhase != batteryPhase) {
    newPowerWatt[batteryPhase] = activePowerWatt[batteryPhase];
  }
  newPowerWatt[modifyPhase] = newPowerWatt[0] - newPowerWatt[batteryPhase];

  // Apply modifications
  for (int phase = 0; phase <=3; phase++) {
    modifiedTelegram = modifyObisPhase(modifiedTelegram, phase, newPowerWatt[phase]);    
  }

  // Recalculate CRC for the modified telegram
  modifiedTelegram = recalculateCRC(modifiedTelegram);
  
  return modifiedTelegram;
}

String P1Modifier::modifyObisValue(const String& telegram, const String& obisCode, float newValue) {
  int startIndex = telegram.indexOf(obisCode);
  if (startIndex == -1) {
    return telegram;
  }
  
  // Find the opening parenthesis after OBIS code
  int openParen = telegram.indexOf('(', startIndex);
  if (openParen == -1) {
    return telegram;
  }
  
  // Find the closing parenthesis
  int closeParen = telegram.indexOf(')', openParen);
  if (closeParen == -1) {
    return telegram;
  }
  
  // Extract current value including unit
  String currentValue = telegram.substring(openParen + 1, closeParen);
  
  // Extract unit (everything after *)
  String unit = "kW";
  int asteriskPos = currentValue.indexOf('*');
  if (asteriskPos != -1) {
    unit = currentValue.substring(asteriskPos + 1);
  }
  
  // Format new value with same precision (3 decimal places for power)
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%.3f*%s", newValue, unit.c_str());
  String newValueStr = String(buffer);
  
  // Replace in telegram
  String result = telegram.substring(0, openParen + 1) + 
                  newValueStr + 
                  telegram.substring(closeParen);
  
  return result;
}

String P1Modifier::modifyObisPhase(const String& originalTelegram, uint8_t phase, float newPowerWatt) {
  String modifiedTelegram = originalTelegram;
  
  String obisDelivered;
  String obisReceived;
  
  switch (phase) {
    case 0:
      obisDelivered = OBIS_POWER_DELIVERED;
      obisReceived = OBIS_POWER_RECEIVED;
      break;
    case 1:
      obisDelivered = OBIS_POWER_DELIVERED_L1;
      obisReceived = OBIS_POWER_RECEIVED_L1;
      break;
    case 2:
      obisDelivered = OBIS_POWER_DELIVERED_L2;
      obisReceived = OBIS_POWER_RECEIVED_L2;
      break;
    case 3:
      obisDelivered = OBIS_POWER_DELIVERED_L3;
      obisReceived = OBIS_POWER_RECEIVED_L3;
      break;
    default:
      return originalTelegram; // Invalid phase
  }
  
  float newPowerkWatt = newPowerWatt / 1000.0;
  
  modifiedTelegram = modifyObisValue(modifiedTelegram, obisDelivered, newPowerkWatt > 0 ? newPowerkWatt : 0.0);
  modifiedTelegram = modifyObisValue(modifiedTelegram, obisReceived, newPowerkWatt < 0 ? -newPowerkWatt : 0.0);
  
  return modifiedTelegram;
}

String P1Modifier::formatPowerValue(float watts) {
  float kw = watts / 1000.0;
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%.3f*kW", kw);
  return String(buffer);
}

// Recalculate and append correct CRC to modified telegram
String P1Modifier::recalculateCRC(const String& telegram) {
  if (telegram.length() < 2) {
    return telegram;  // Telegram too short
  }
  
  // Remove old CRC if present (4 chars after '!')
  int exclamationPos = telegram.lastIndexOf('!');
  if (exclamationPos == -1 || exclamationPos < 2) {
    return telegram;  // No exclamation mark, return as-is
  }
  
  // Extract data up to and including '!'
  String dataForCRC = telegram.substring(0, exclamationPos + 1);
  
  // Calculate new CRC
  String newCRC = P1Parser::calculateCRC16(dataForCRC);
  
  if (newCRC.length() != 4) {
    return telegram;  // CRC calculation failed
  }
  
  // Append new CRC
  return dataForCRC + newCRC;
}