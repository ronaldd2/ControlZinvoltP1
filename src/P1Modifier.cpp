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
  current_mode_ = MODE_UNMODIFIED;
  battery_phase_ = 1;
  modify_phase_ = 1;
  single_phase_meter_mode_ = false;
  force_power_ = 2000.0;  // Default 2kW
  power_setpoint_ = 0.0;  // Default 0W (neutral)
  external_control_power_ = 0.0;
  external_control_last_update_ = 0;
  self_use_limit_threshold_ = 20.0;   // Default 20W
  self_use_smoothing_factor_ = 0.3;   // Default moderate smoothing
  last_smoothed_power_ = 0.0;
  last_smooth_update_time_ = 0;
  config_ = nullptr;
  noise_ = -1;               // Start deterministic noise cycle (-1,0,+1)
  force_integrator_ = 0.0f;  // Ensure stable start for force modes
  last_power_watt_ = 1;
  
  // Lag-aware initialization
  previous_battery_power_ = 0.0;
  last_battery_update_ = 0;
  current_direction_ = 0.0;  // 0 = neutral, negative = charging, positive = discharging

  battery_mode_ = BM_OFF;
  last_direction_change_ = 0;
  
  // Telegram interval tracking
  last_modify_time_ = 0;
  telegram_interval_sec_ = 10.0;  // Default to 10 seconds
}

String P1Modifier::getModeString() const {
  switch (current_mode_) {
    case MODE_UNMODIFIED: return "Unmodified";
    case MODE_BATTERY_OFF: return "Battery Off";
    case MODE_FORCE_CHARGE: return "Force Charge";
    case MODE_FORCE_DISCHARGE: return "Force Discharge";
    case MODE_POWER_CONTROL: return "Power Control [" + String(power_setpoint_,0) + "W]";
    case MODE_CHARGE_ONLY: return "Charge Only";
    case MODE_DISCHARGE_ONLY: return "Discharge Only";
    case MODE_EXTERNAL_CONTROL: return "External Control";
    case MODE_OPTIMIZE: return "Optimize [" + String(optimize_setpoint_,1) + "W]";
    default: return "Unknown";
  }
}

String P1Modifier::modify(const String& originalTelegram, const P1Parser& parser, float batteryPower) {
  // If unmodified mode, return original
  if (current_mode_ == MODE_UNMODIFIED && !single_phase_meter_mode_) {
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
  if (last_modify_time_ > 0) {
    float intervalMs = now - last_modify_time_;
    float detectedIntervalSec = intervalMs / 1000.0;
    
    // Use exponential moving average for smooth interval detection
    telegram_interval_sec_ = (telegram_interval_sec_ * 0.9) + (detectedIntervalSec * 0.1);
  }
  last_modify_time_ = now;
  
  if (last_battery_update_ == 0) {
    last_battery_update_ = now;
    previous_battery_power_ = localBatteryPower;
  }
  
  float powerDelta = localBatteryPower - previous_battery_power_;
  float direction = getPowerDirection(localBatteryPower);
  
  // Detect direction changes (with lag awareness - only confirm after 30 seconds)
  if ((direction != 0.0) && ((direction * current_direction_) <= 0.0)) {
    // Direction might be changing
    if (now - last_direction_change_ > 30000) {
      // Confirmed direction change after 30s lag buffer
      current_direction_ = direction;
      last_direction_change_ = now;
    }
  }
  if (batteryPower == 0.0) {
    battery_mode_ = BM_OFF;
  }
  if (batteryPower < -10.0) {
    battery_mode_ = BM_CHARGING;
  } 
  if (batteryPower > 10.0) {
    battery_mode_ = BM_DISCHARGING;
  }

  previous_battery_power_ = localBatteryPower;
  last_battery_update_ = now;
  
  switch (current_mode_) {
    case MODE_BATTERY_OFF:
      // Prevent charging/discharging: reduce power to near zero
      newPowerWatt[0] = -localBatteryPower*0.3;  // Minimal power to avoid division by zero
      break;
      
    case MODE_CHARGE_ONLY:
      // Only allow charging (prevent discharging)
      // Use trend to avoid over-correcting due to lag
      // If trend shows discharging (positive), apply counter-power
      if (battery_mode_ == BM_DISCHARGING) {
        // Battery is discharging - gradually reduce export to stop discharging
        // Use smaller adjustments to account for 15-20s lag
        float adjustment = abs(localBatteryPower)*0.5;
        newPowerWatt[0] = -adjustment;
      } else { 
        if (activePowerWatt[0] > 0) {
          // there is power consumption, prevent mode change to discharging
          if (battery_mode_ == BM_OFF) {
          // If battery is charging or neutral, allow normal charging
            newPowerWatt[0]  = 0.0f; // Prevent mode change to discharging
          } else {
            // Battery is charging - allow normal charging
            newPowerWatt[0] = activePowerWatt[0]/3.0;
          }
        }  else {
          // we deliver power to the grid
        }
      }
      break;
      
    case MODE_DISCHARGE_ONLY:
      // Only allow discharging (prevent charging)
      // If trend shows charging (negative), apply counter-power
      if (battery_mode_ == BM_CHARGING) {
        // Battery is charging - gradually reduce import to force discharging
        float adjustment = abs(localBatteryPower)*0.5;
        newPowerWatt[0] = adjustment;
      } else {
        if (activePowerWatt[0] < 0) {
          // there is power being delivered to the grid, prevent mode change to charging
          if (battery_mode_ == BM_OFF) {
            // If battery is discharging or neutral, allow normal discharging
            newPowerWatt[0] = 0.0f; // Prevent charging
          } else {
            // Battery is discharging - allow normal discharging
            newPowerWatt[0] = activePowerWatt[0]/3.0;
          }
        }  else {
          // we consume power from the grid
        }
      } 
      break;
      
    case MODE_EXTERNAL_CONTROL:
      // External control via REST API
      logPrint("[EXTERNAL_CONTROL] ");
    
      if (isExternalControlValid()) {
        logPrintln(String(external_control_power_,0) + " W");
        newPowerWatt[0] = external_control_power_;
      } else {
        logPrintln(" - external control data stale, no adjustment applied");
      }
      break;
      
    case MODE_FORCE_CHARGE:
      // Force battery to charge by showing high grid consumption
      // Target: localBatteryPower should be -force_power_ (e.g., -1000W = charging at 1000W)
      // Control logic:
      //   - If batteryPower = -900W (less charging than -1000W target), error = -900 - (-1000) = +100W
      //   - Positive error means undercharging -> need to INCREASE grid consumption (positive newPowerWatt)
      //   - To make battery charge more, show the grid meter that home is consuming more power
      // 
      // Integrator accumulates error over time for steady-state correction
      {
        float chargeError = localBatteryPower - (-force_power_);  // Positive when undercharging
        
        force_integrator_ = updateIntegrator(force_integrator_, 5, chargeError > 50, chargeError < -50);

        if ((chargeError >= -50) && (chargeError <= 50)) {
          // Within deadband, slowly decay integrator to avoid windup
          force_integrator_ = constrain(force_integrator_, -10, 10);
          force_integrator_ = updateIntegrator(force_integrator_, 1, force_integrator_ < 0, force_integrator_ > 0);
        }
        
        // Limit integrator to prevent windup
        force_integrator_ = constrain(force_integrator_,-50, 50);
        
        // Apply proportional + integral control:
        // - Proportional term: chargeError/3 provides immediate response (damped by /3 for stability)
        // - Integral term: force_integrator_ corrects steady-state offset
        newPowerWatt[0] = -force_integrator_ - (chargeError / 3.0f);
        
        logPrint("[FORCE_CHARGE] BatPower=");
        logPrint(String(localBatteryPower, 0));
        logPrint("W, Error=");
        logPrint(String(chargeError, 0));
        logPrint("W, Integrator=");
        logPrint(String(force_integrator_, 0));
        logPrint(", Output=");
        logPrint(String(newPowerWatt[0], 0));
        logPrintln("W");
      }
      break;
      
    case MODE_FORCE_DISCHARGE:
      // Force battery to discharge by showing high grid export (low/negative power)
      // Target: localBatteryPower should be +force_power_ (e.g., +1000W = discharging at 1000W)
      // Control logic:
      //   - If batteryPower = +900W (less discharging than +1000W target), error = 900 - 1000 = -100W
      //   - Negative error means under-discharging -> need to DECREASE grid power (show more export)
      //   - To make battery discharge more, show the grid meter that home needs less power / exports more
      //
      // Integrator accumulates error over time for steady-state correction
      {
        float dischargeError = localBatteryPower - force_power_;  // Negative when under-discharging
        
        force_integrator_ = updateIntegrator(force_integrator_, 5, dischargeError > 50, dischargeError < -50);

        if ((dischargeError >= -50) && (dischargeError <= 50)) {
          // Within deadband, slowly decay integrator to avoid windup
          force_integrator_ = constrain(force_integrator_, -10, 10);
          force_integrator_ = updateIntegrator(force_integrator_, 1, force_integrator_ < 0, force_integrator_ > 0);
        }
        
        // Limit integrator to prevent windup
        force_integrator_ = constrain(force_integrator_, -50, 50);
        
        // Apply proportional + integral control:
        // - Proportional term: dischargeError/3 provides immediate response (damped by /3)
        // - Integral term: force_integrator_ corrects steady-state offset
        newPowerWatt[0] = -force_integrator_ - (dischargeError / 3.0f);
        
        logPrint("[FORCE_DISCHARGE] BatPower=");
        logPrint(String(localBatteryPower, 0));
        logPrint("W, Error=");
        logPrint(String(dischargeError, 0));
        logPrint("W, Integrator=");
        logPrint(String(force_integrator_, 0));
        logPrint(", Output=");
        logPrint(String(newPowerWatt[0], 0));
        logPrintln("W");
      }
      break;
      
    case MODE_POWER_CONTROL:
      // Control battery to specific power setpoint (positive = discharge, negative = charge)
      // Target: localBatteryPower should match power_setpoint_
      // Control logic: same as force modes but uses power_setpoint_ as target
      {
        float offset = -self_use_limit_threshold_;    
        if (abs(activePowerWatt[0]) > 100.0) {
          power_setpoint_ = activePowerWatt[0] + offset;
        }
        power_setpoint_ = updateIntegrator(
            power_setpoint_,
            1,  // step
            activePowerWatt[0] < (offset - 5),      // Increase adjustment when exporting
            activePowerWatt[0] > (offset + 25)      // Decrease adjustment when importing
        ); 

        float powerError = -localBatteryPower - power_setpoint_;
        
        force_integrator_ = updateIntegrator(force_integrator_, 5, powerError > 50, powerError < -50);

        if ((powerError >= -50) && (powerError <= 50)) {
          // Within deadband, slowly decay integrator to avoid windup
          force_integrator_ = constrain(force_integrator_, -10, 10);
          force_integrator_ = updateIntegrator(force_integrator_, 1, force_integrator_ < 0, force_integrator_ > 0);
        }
        
        // Limit integrator to prevent windup
        force_integrator_ = constrain(force_integrator_,-50, 50);
        
        // Apply proportional + integral control:
        // - Proportional term: powerError/3 provides immediate response
        // - Integral term: force_integrator_ corrects steady-state offset
        newPowerWatt[0] = -force_integrator_ - (powerError / 3.0f);
        
        logPrint("[POWER_CONTROL] BatPower=");
        logPrint(String(localBatteryPower, 0));
        logPrint("W, Setpoint=");
        logPrint(String(power_setpoint_, 0));
        logPrint("W, Error=");
        logPrint(String(powerError, 0));
        logPrint("W, Integrator=");
        logPrint(String(force_integrator_, 0));
        logPrint(", Output=");
        logPrint(String(newPowerWatt[0], 0));
        logPrintln("W");
      }
      break;
      
    case MODE_OPTIMIZE: {
      // Use DomoticzLogic for smart battery control
      if (config_) {
        // Ensure DomoticzLogic has config pointer
        domoticz_logic_.setConfig(config_);
        
        float currentP1Delivery = -activePowerWatt[0];  // Current grid delivery (positive = export)
        float solar = config_->actualSolarPower;
        float evaCharge = (localBatteryPower < -10) ? abs(localBatteryPower) : 0;
        float evaDischarge = (localBatteryPower > 10) ? localBatteryPower : 0;
        float soc = config_->batterySOC;
        int seconds = millis() / 1000;
        
        // Calculate power adjustment using DomoticzLogic
        float adjustment = domoticz_logic_.calculate(currentP1Delivery, solar, 
                                                     evaCharge, evaDischarge, soc, seconds);
        
        optimize_setpoint_ = domoticz_logic_.getDeliverySetpoint();
        power_adjustment_ = adjustment;
        
        // Apply adjustment to power value
        newPowerWatt[0] = activePowerWatt[0] + adjustment;
        
        logPrint("[OPTIMIZE] Current=");
        logPrint(String(currentP1Delivery, 1));
        logPrint("W, Setpoint=");
        logPrint(String(optimize_setpoint_, 1));
        logPrint("W, Adjustment=");
        logPrint(String(adjustment, 1));
        logPrint("W, Solar=");
        logPrint(String(solar, 1));
        logPrint("W, Battery=");
        logPrint(String(localBatteryPower, 1));
        logPrintln("W");
        
        // Update integrator every minute
        static unsigned long lastMinuteUpdate = 0;
        if (millis() - lastMinuteUpdate >= 60000) {
          domoticz_logic_.updateMinute(currentP1Delivery);
          lastMinuteUpdate = millis();
        }
      } else {
        logPrintln("[OPTIMIZE] ERROR: Config not set! Use setConfig().");
      }
      break;
    }
      
    default:
      break;
  }
  logPrint(" NewPower=");
  logPrint(String(newPowerWatt[0],1));
  logPrintln("W");


  // Add small noise AFTER mode calculations to help battery controller detect changes
  // This prevents wild fluctuations from integrators while ensuring value changes
  //if (++noise_ > 1) noise_ = -1; // Cycle through -1, 0, +1
  //newPowerWatt[0] += noise_ ; // ±1W noise
  if (newPowerWatt[0] == last_power_watt_) {
    // Ensure power changes slightly each telegram to help battery controller
    newPowerWatt[0] += 1; // Change by at least 1W
  }
  last_power_watt_ = newPowerWatt[0];
  
  // Distribute to maintain correct 3-phase sum
  // Calculate delta needed to reach target total
  float currentSum = activePowerWatt[1] + activePowerWatt[2] + activePowerWatt[3];
  float targetTotal = newPowerWatt[0];
  float deltaNeeded = targetTotal - currentSum;
  
  if (modify_phase_ < 1 || modify_phase_ > 3) {
    // Fallback safety: if modify_phase_ is invalid, default to L1
    modify_phase_ = 1;
  }
  
  // Apply all the change to modify_phase_, keep other phases at original values
  newPowerWatt[modify_phase_] = activePowerWatt[modify_phase_] + deltaNeeded;
  // Other phases already set to original values at start of function

  if (single_phase_meter_mode_) {
    // Expose telegram as single-phase while keeping total power correct
    newPowerWatt[1] = targetTotal;
    newPowerWatt[2] = 0.0f;
    newPowerWatt[3] = 0.0f;
  }

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
  if (telegram_interval_sec_ < 5.0 && telegram_interval_sec_ > 0.5) {
    // For ~1 second intervals, divide step by 10
    adjustedStep = step * (telegram_interval_sec_ / 10.0);
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
  return dataForCRC + newCRC + "\r\n";
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