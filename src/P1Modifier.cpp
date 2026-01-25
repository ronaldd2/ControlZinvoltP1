/*
 * P1Modifier.cpp - Implementation of P1 Modifier
 */

#include "P1Modifier.h"
#include "HomeAssistant.h"

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
  selfUseLimitThreshold = 20.0;   // Default 20W
  selfUseSmoothingFactor = 0.3;   // Default moderate smoothing
  lastSmoothedPower = 0.0;
  lastSmoothUpdateTime = 0;
  _noise = -1;               // Start deterministic noise cycle (-1,0,+1)
  _force_integrator = 0.0f;  // Ensure stable start for force modes
  
  // Lag-aware initialization
  previousBatteryPower = 0.0;
  lastBatteryUpdate = 0;
  currentDirection = 0.0;  // 0 = neutral, negative = charging, positive = discharging

  batteryMode = BM_OFF;
  lastDirectionChange = 0;
  
  // Telegram interval tracking
  lastModifyTime = 0;
  telegramIntervalSec = 10.0;  // Default to 10 seconds
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
    case MODE_SELF_USE_LIMITER: return "Self-Use Limiter";
    default: return "Unknown";
  }
}

String P1Modifier::modify(const String& originalTelegram, const P1Parser& parser, float batteryPower) {
  // If unmodified mode, return original
  if (currentMode == MODE_UNMODIFIED) {
    return originalTelegram;
  }
  
  // Take local snapshot of batteryPower to avoid race conditions during processing
  float localBatteryPower = batteryPower;
  
  String modifiedTelegram = originalTelegram;
  
  float activePowerWatt[4];
  float newPowerWatt[4] = {0, 0, 0, 0};
  for (int phase = 0; phase <=3; phase++) {
    activePowerWatt[phase] = parser.getActivePower(phase)*1000.0; // Convert kW to W  
    newPowerWatt[phase] = activePowerWatt[phase];
  }

  // Update battery power trend tracking (accounting for system lag)
  unsigned long now = millis();
  
  // Detect telegram interval for adaptive integrator step sizing
  if (lastModifyTime > 0) {
    float intervalMs = now - lastModifyTime;
    float detectedIntervalSec = intervalMs / 1000.0;
    
    // Use exponential moving average for smooth interval detection
    telegramIntervalSec = (telegramIntervalSec * 0.9) + (detectedIntervalSec * 0.1);
  }
  lastModifyTime = now;
  
  if (lastBatteryUpdate == 0) {
    lastBatteryUpdate = now;
    previousBatteryPower = localBatteryPower;
  }
  
  float powerDelta = localBatteryPower - previousBatteryPower;
  float direction = getPowerDirection(localBatteryPower);
  
  // Detect direction changes (with lag awareness - only confirm after 30 seconds)
  if ((direction != 0.0) && ((direction * currentDirection) <= 0.0)) {
    // Direction might be changing
    if (now - lastDirectionChange > 30000) {
      // Confirmed direction change after 30s lag buffer
      currentDirection = direction;
      lastDirectionChange = now;
    }
  }
  if (batteryPower == 0.0) {
    batteryMode = BM_OFF;
  }
  if (batteryPower < -10.0) {
    batteryMode = BM_CHARGING;
  } 
  if (batteryPower > 10.0) {
    batteryMode = BM_DISCHARGING;
  }

  previousBatteryPower = localBatteryPower;
  lastBatteryUpdate = now;
  
  switch (currentMode) {
    case MODE_OFF:
      // Prevent charging/discharging: reduce power to near zero
      newPowerWatt[0] = -localBatteryPower*0.3;  // Minimal power to avoid division by zero
      break;
      
    case MODE_CHARGE_ONLY:
      // Only allow charging (prevent discharging)
      // Use trend to avoid over-correcting due to lag
      // If trend shows discharging (positive), apply counter-power
      if (batteryMode == BM_DISCHARGING) {
        // Battery is discharging - gradually reduce export to force charging
        // Use smaller adjustments to account for 15-20s lag
        float adjustment = abs(localBatteryPower)*0.5;
        newPowerWatt[0] = -adjustment;
      } else if (batteryMode == BM_OFF) {
        // If battery is charging or neutral, allow normal charging
        newPowerWatt[0]  = 0.0f; // Prevent discharging
      }
      break;
      
    case MODE_DISCHARGE_ONLY:
      // Only allow discharging (prevent charging)
      // If trend shows charging (negative), apply counter-power
      if (batteryMode == BM_CHARGING) {
        // Battery is charging - gradually reduce import to force discharging
        float adjustment = abs(localBatteryPower)*0.5;
        newPowerWatt[0] = adjustment;
      } else if (batteryMode == BM_OFF) {
        // If battery is discharging or neutral, allow normal discharging
        newPowerWatt[0] = 0.0f; // Prevent charging
      }
      break;
      
    case MODE_EXTERNAL_CONTROL:
      // External control via REST API
      if (isExternalControlValid()) {
        newPowerWatt[0] = externalControlPower;
      }
      break;
      
    case MODE_FORCE_CHARGE:
      // Force battery to charge by showing high grid consumption
      // Target: localBatteryPower should be -forcePower (e.g., -1000W = charging at 1000W)
      // Control logic:
      //   - If batteryPower = -900W (less charging than -1000W target), error = -900 - (-1000) = +100W
      //   - Positive error means undercharging -> need to INCREASE grid consumption (positive newPowerWatt)
      //   - To make battery charge more, show the grid meter that home is consuming more power
      // 
      // Integrator accumulates error over time for steady-state correction
      {
        float chargeError = localBatteryPower - (-forcePower);  // Positive when undercharging
        
        _force_integrator = updateIntegrator(_force_integrator, 5, chargeError > 50, chargeError < -50);

        if ((chargeError >= -50) && (chargeError <= 50)) {
          // Within deadband, slowly decay integrator to avoid windup
          _force_integrator = constrain(_force_integrator, -10, 10);
          _force_integrator = updateIntegrator(_force_integrator, 1, _force_integrator < 0, _force_integrator > 0);
        }
        
        // Limit integrator to prevent windup
        _force_integrator = constrain(_force_integrator,-50, 50);
        
        // Apply proportional + integral control:
        // - Proportional term: chargeError/3 provides immediate response (damped by /3 for stability)
        // - Integral term: _force_integrator corrects steady-state offset
        newPowerWatt[0] = -_force_integrator - (chargeError / 3.0f);
        
        logPrint("[FORCE_CHARGE] BatPower=");
        logPrint(String(localBatteryPower, 0));
        logPrint("W, Error=");
        logPrint(String(chargeError, 0));
        logPrint("W, Integrator=");
        logPrint(String(_force_integrator, 0));
        logPrint(", Output=");
        logPrint(String(newPowerWatt[0], 0));
        logPrintln("W");
      }
      break;
      
    case MODE_FORCE_DISCHARGE:
      // Force battery to discharge by showing high grid export (low/negative power)
      // Target: localBatteryPower should be +forcePower (e.g., +1000W = discharging at 1000W)
      // Control logic:
      //   - If batteryPower = +900W (less discharging than +1000W target), error = 900 - 1000 = -100W
      //   - Negative error means under-discharging -> need to DECREASE grid power (show more export)
      //   - To make battery discharge more, show the grid meter that home needs less power / exports more
      //
      // Integrator accumulates error over time for steady-state correction
      {
        float dischargeError = localBatteryPower - forcePower;  // Negative when under-discharging
        
        _force_integrator = updateIntegrator(_force_integrator, 5, dischargeError > 50, dischargeError < -50);

        if ((dischargeError >= -50) && (dischargeError <= 50)) {
          // Within deadband, slowly decay integrator to avoid windup
          _force_integrator = constrain(_force_integrator, -10, 10);
          _force_integrator = updateIntegrator(_force_integrator, 1, _force_integrator < 0, _force_integrator > 0);
        }
        
        // Limit integrator to prevent windup
        _force_integrator = constrain(_force_integrator, -50, 50);
        
        // Apply proportional + integral control:
        // - Proportional term: dischargeError/3 provides immediate response (damped by /3)
        // - Integral term: _force_integrator corrects steady-state offset
        newPowerWatt[0] = -_force_integrator - (dischargeError / 3.0f);
        
        logPrint("[FORCE_DISCHARGE] BatPower=");
        logPrint(String(localBatteryPower, 0));
        logPrint("W, Error=");
        logPrint(String(dischargeError, 0));
        logPrint("W, Integrator=");
        logPrint(String(_force_integrator, 0));
        logPrint(", Output=");
        logPrint(String(newPowerWatt[0], 0));
        logPrintln("W");
      }
      break;
      
    case MODE_SELF_USE_LIMITER: {

      float offset = selfUseLimitThreshold;
      
      if (batteryMode == BM_CHARGING) {
        offset += abs(localBatteryPower)*0.1; // Increase offset when charging
      } 
      logPrint("[LIMITER] offset=");
      logPrint(String(offset,1));
      logPrint("W, selfUseLimitThreshold=");
      logPrint(String(selfUseLimitThreshold,1));
      float factor = 1.0;
      float lowThreshold = 100.0;
      float maxFactor = 3.0;
      float k = 0.01; // Adjust steepness of the curve
    
      if (abs(newPowerWatt[0]) > lowThreshold) {
          float t = (abs(newPowerWatt[0]) - lowThreshold);
          // Exponentiële stijging richting maxFactor
          factor = 1.0 + (maxFactor - 1.0) * (1.0 - std::exp(-k * t));
          _powerAdjustment = -offset;

      }
      logPrint(" factor=");
      logPrint(String(factor, 3));
      newPowerWatt[0] = (activePowerWatt[0] /factor) - _powerAdjustment;

      if ((activePowerWatt[0] > (-offset - 5)) && (activePowerWatt[0] <(-offset+ 25))) {
        // we are fine, set the output to zero
        logPrint(" within deadband, no adjustment ");
        newPowerWatt[0]=0;
      } else {
        if (abs(activePowerWatt[0]) <= (1.5*offset)) {
          // small error, small step
          _powerAdjustment = updateIntegrator(
            _powerAdjustment,
            1,  // step
            activePowerWatt[0] < (-offset - 5),      // Increase adjustment when exporting
            activePowerWatt[0] > (-offset + 25)      // Decrease adjustment when importing
          );
          constrain(_powerAdjustment, -offset, offset);
          newPowerWatt[0]  =- _powerAdjustment;
          logPrint(" powerAdj=");
          logPrint(String(_powerAdjustment,1));
        }     
      }
      logPrint(" NewPower=");
      logPrint(String(newPowerWatt[0],1));
      logPrintln("W");
      break;
    }
      
    default:
      break;
  }
  
  // Add small noise AFTER mode calculations to help battery controller detect changes
  // This prevents wild fluctuations from integrators while ensuring value changes
  if (++_noise > 1) _noise = -1; // Cycle through -1, 0, +1
  newPowerWatt[0] += _noise ; // ±1W noise
  
  // Distribute to maintain correct 3-phase sum
  // Calculate delta needed to reach target total
  float currentSum = activePowerWatt[1] + activePowerWatt[2] + activePowerWatt[3];
  float targetTotal = newPowerWatt[0];
  float deltaNeeded = targetTotal - currentSum;
  
  if (modifyPhase < 1 || modifyPhase > 3) {
    // Fallback safety: if modifyPhase is invalid, default to L1
    modifyPhase = 1;
  }
  
  // Apply all the change to modifyPhase, keep other phases at original values
  newPowerWatt[modifyPhase] = activePowerWatt[modifyPhase] + deltaNeeded;
  // Other phases already set to original values at start of function

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

float P1Modifier::updateIntegrator(float currentValue, float step, bool applyPos, bool applyNeg) {
  // Generic integrator helper: optionally apply symmetric step in both directions
  // Adjust step based on telegram interval (10s baseline, scale for faster intervals)
  float adjustedStep = step;
  if (telegramIntervalSec < 5.0 && telegramIntervalSec > 0.5) {
    // For ~1 second intervals, divide step by 10
    adjustedStep = step * (telegramIntervalSec / 10.0);
  }
  
  if (applyPos) {
    currentValue += adjustedStep;
  }
  if (applyNeg) {
    currentValue -= adjustedStep;
  }
  return currentValue;
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

// Get battery power direction trend accounting for system lag
// Returns: negative = charging trend, positive = discharging trend, 0 = neutral/no activity
float P1Modifier::getPowerDirection(float batteryPower) {
  // Hysteresis thresholds to avoid noise oscillation (system lag makes exact values unreliable)
  const float CHARGE_THRESHOLD = -200.0;  // Charging if < -200W
  const float DISCHARGE_THRESHOLD = 200.0;  // Discharging if > 200W
  
  if (batteryPower < CHARGE_THRESHOLD) {
    return -1.0;  // Charging trend
  } else if (batteryPower > DISCHARGE_THRESHOLD) {
    return 1.0;  // Discharging trend
  } else {
    return 0.0;  // Neutral - battery relatively idle
  }
}