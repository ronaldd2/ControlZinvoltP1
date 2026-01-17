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
  forcePower = 3000.0;  // Default 3kW
}

String P1Modifier::getModeString() const {
  switch (currentMode) {
    case MODE_UNMODIFIED: return "Unmodified";
    case MODE_OFF: return "Off";
    case MODE_FORCE_CHARGE: return "Force Charge";
    case MODE_FORCE_DISCHARGE: return "Force Discharge";
    case MODE_CHARGE_ONLY: return "Charge Only";
    case MODE_DISCHARGE_ONLY: return "Discharge Only";
    default: return "Unknown";
  }
}

String P1Modifier::modify(const String& originalTelegram, const P1Parser& parser) {
  // If unmodified mode, return original
  if (currentMode == MODE_UNMODIFIED) {
    return originalTelegram;
  }
  
  String modifiedTelegram = originalTelegram;
  
  // Determine which OBIS codes to modify based on modifyPhase
  String obisDelivered, obisReceived;
  float currentPowerDelivered = 0;
  float currentPowerReceived = 0;
  
  switch (modifyPhase) {
    case 1:
      obisDelivered = OBIS_POWER_DELIVERED_L1;
      obisReceived = OBIS_POWER_RECEIVED_L1;
      currentPowerDelivered = parser.getActivePowerL1() > 0 ? parser.getActivePowerL1() : 0;
      currentPowerReceived = parser.getActivePowerL1() < 0 ? -parser.getActivePowerL1() : 0;
      break;
    case 2:
      obisDelivered = OBIS_POWER_DELIVERED_L2;
      obisReceived = OBIS_POWER_RECEIVED_L2;
      currentPowerDelivered = parser.getActivePowerL2() > 0 ? parser.getActivePowerL2() : 0;
      currentPowerReceived = parser.getActivePowerL2() < 0 ? -parser.getActivePowerL2() : 0;
      break;
    case 3:
      obisDelivered = OBIS_POWER_DELIVERED_L3;
      obisReceived = OBIS_POWER_RECEIVED_L3;
      currentPowerDelivered = parser.getActivePowerL3() > 0 ? parser.getActivePowerL3() : 0;
      currentPowerReceived = parser.getActivePowerL3() < 0 ? -parser.getActivePowerL3() : 0;
      break;
    default:
      return originalTelegram;
  }
  
  // Modify based on mode
  float newPowerDelivered = currentPowerDelivered;
  float newPowerReceived = currentPowerReceived;
  
  switch (currentMode) {
    case MODE_OFF:
      // Show zero power (no charging/discharging)
      newPowerDelivered = 0.001;  // Small value to avoid division by zero
      newPowerReceived = 0.0;
      break;
      
    case MODE_FORCE_CHARGE:
      // Show high consumption to force charging
      newPowerDelivered = currentPowerDelivered + (forcePower / 1000.0);  // Convert W to kW
      newPowerReceived = 0.0;
      break;
      
    case MODE_FORCE_DISCHARGE:
      // Show high generation to force discharging
      newPowerDelivered = 0.001;
      newPowerReceived = currentPowerReceived + (forcePower / 1000.0);  // Convert W to kW
      break;
      
    case MODE_CHARGE_ONLY:
      // Only allow charging (future - needs battery SOC info)
      if (currentPowerReceived > 0) {
        newPowerReceived = 0.0;
      }
      break;
      
    case MODE_DISCHARGE_ONLY:
      // Only allow discharging (future - needs battery SOC info)
      if (currentPowerDelivered > currentPowerReceived) {
        newPowerDelivered = currentPowerReceived;
      }
      break;
      
    default:
      break;
  }
  
  // Apply modifications
  modifiedTelegram = modifyObisValue(modifiedTelegram, obisDelivered, newPowerDelivered);
  modifiedTelegram = modifyObisValue(modifiedTelegram, obisReceived, newPowerReceived);
  
  // Also update total power values
  float totalDelivered = parser.getActivePowerL1() + parser.getActivePowerL2() + parser.getActivePowerL3();
  float totalReceived = parser.getActivePowerDeliveredL1() + parser.getActivePowerDeliveredL2() + parser.getActivePowerDeliveredL3();
  
  // Adjust totals based on phase modification
  if (modifyPhase == 1) {
    totalDelivered = totalDelivered - currentPowerDelivered + newPowerDelivered;
    totalReceived = totalReceived - currentPowerReceived + newPowerReceived;
  }
  
  modifiedTelegram = modifyObisValue(modifiedTelegram, OBIS_POWER_DELIVERED, totalDelivered > 0 ? totalDelivered : 0.001);
  modifiedTelegram = modifyObisValue(modifiedTelegram, OBIS_POWER_RECEIVED, totalReceived > 0 ? totalReceived : 0.0);
  
  // Recalculate CRC if needed
  // Note: Some P1 telegrams have CRC, some don't
  
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

String P1Modifier::formatPowerValue(float watts) {
  float kw = watts / 1000.0;
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%.3f*kW", kw);
  return String(buffer);
}
