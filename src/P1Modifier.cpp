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
  optimize_setpoint_ = -20.0f;
  filtered_delivery_w_ = 0.0f;
  filter_initialized_ = false;
  optimize_integrator_ = 0.0f;
  optimize_last_error_w_ = 0.0f;
  optimize_neutral_hold_ = false;
  optimize_trace_actual_w_ = 0.0f;
  optimize_trace_scaled_actual_w_ = 0.0f;
  optimize_trace_error_w_ = 0.0f;
  optimize_trace_command_w_ = 0.0f;
  optimize_trace_target_w_ = 0.0f;
  optimize_trace_filtered_w_ = 0.0f;
  optimize_trace_integrator_w_ = 0.0f;
  optimize_trace_adjust_divisor_ = 1.0f;
  config_ = nullptr;
  force_integrator_ = 0.0f;  // Ensure stable start for force modes
  last_power_watt_ = 1;
  anti_repeat_add_positive_ = true;
  
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

void P1Modifier::calculateOptimizeSetpoint() {
  if (!config_) {
    return;
  }

  const float solarPowerW = max(0.0f, config_->actualSolarPower);

  const float baseSetpoint = config_->optimizeDeliverySetpoint;
  const float highSetpoint = config_->optimizeHighSolarSetpoint;
  const float minSolar = max(0.1f, config_->optimizeMinSolarPower);
  const float solarThreshold = max(minSolar + 1.0f, config_->optimizeSolarThreshold);

  float targetDeliveryW = baseSetpoint;
  if (solarPowerW >= solarThreshold) {
    targetDeliveryW = highSetpoint;
  } else if (solarPowerW > minSolar) {
    float ratio = (solarPowerW - minSolar) / (solarThreshold - minSolar);
    targetDeliveryW = baseSetpoint + (highSetpoint - baseSetpoint) * ratio;
  }

  // Meter sign convention: export is negative.
  optimize_setpoint_ = -targetDeliveryW;
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

  // Keep optimize setpoint up-to-date for all modes (reusable outside MODE_OPTIMIZE).
  calculateOptimizeSetpoint();
  
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
            newPowerWatt[0] = 0.0f; // Prevent mode change to discharging
          } else {
            // Battery is charging - allow normal charging
            newPowerWatt[0] = activePowerWatt[0]/3.0;
          }
        } else {
          // we deliver power to the grid
          newPowerWatt[0] = activePowerWatt[0]/3.0;
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
          newPowerWatt[0] = activePowerWatt[0]/3.0;
          if (newPowerWatt[0] < -optimize_setpoint_) {
            // Make sure we control at the setpoint
            newPowerWatt[0] =-optimize_setpoint_/3.0;
          }
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

      float adjustDivisor = max(1.0f, config_->optimizeAdjustDivisor);
      if ((activePowerWatt[0] > 0.7f * optimize_setpoint_) || (activePowerWatt[0] < 1.3f * optimize_setpoint_)) {
        newPowerWatt[0] = (activePowerWatt[0] - optimize_setpoint_)/adjustDivisor; 
        if (abs(newPowerWatt[0]) < 5.0f) {
          // If very close to setpoint, apply minimum adjustment to overcome noise and prevent oscillation around target.
          if (newPowerWatt[0] > 1.0f) {
            newPowerWatt[0] = 5.0f; // Minimum adjustment to overcome noise when undercharging
          } 
          if (newPowerWatt[0] < -1.0f) {
            newPowerWatt[0] = -5.0f; // Minimum adjustment to overcome noise when overcharging
          }
        }
        if ((newPowerWatt[0] > -50.0f) && (activePowerWatt[0] <-70.0f) && (battery_mode_ == BM_OFF )) {
          newPowerWatt[0] = -50.0f; // start charging ealier as the limitation might prevent this.
        }

      } else {
        // within 30% of setpoint, hold steady to avoid oscillation from noise and lag
        newPowerWatt[0] = 0.0f;
      }

      break;
      if (config_) {
        float currentP1Power = activePowerWatt[0];  // meter sign: negative = export to grid
        float adjustDivisor = max(1.0f, config_->optimizeAdjustDivisor);
        float scaledCurrentP1Power = currentP1Power / adjustDivisor;

        // Error for optimize control in meter sign convention.
        // Negative/positive values indicate how far measured grid power is from target.
        float errorW = optimize_setpoint_ - scaledCurrentP1Power;
        float realErrorW = optimize_setpoint_ - currentP1Power;
        float realDeadbandW = max(0.0f, config_->optimizeErrorDeadband);
        bool realInDeadband = fabs(realErrorW) <= realDeadbandW;

        // Neutral zone + hysteresis to avoid charge/discharge oscillation from delayed response.
        // Inside this window we command zero correction so the battery can settle.
        float tolLow = min(config_->optimizeToleranceLow, config_->optimizeToleranceHigh);
        float tolHigh = max(config_->optimizeToleranceLow, config_->optimizeToleranceHigh);
        float hysteresisMargin = max(1.0f, config_->optimizeHysteresisDelivery);

        if (!optimize_neutral_hold_) {
          if (errorW >= tolLow && errorW <= tolHigh) {
            optimize_neutral_hold_ = true;
          }
        } else {
          if (errorW < (tolLow - hysteresisMargin) || errorW > (tolHigh + hysteresisMargin)) {
            optimize_neutral_hold_ = false;
          }
        }

        // Very small slow integrator with anti-windup.
        // Scale by telegram interval and keep contribution intentionally tiny.
        float dtSec = constrain(telegram_interval_sec_, 0.5f, 60.0f);
        float integratorStepPerMin = max(0.0f, config_->optimizeIntegratorStep) * 0.1f;
        float integratorStep = integratorStepPerMin * (dtSec / 60.0f);
        float integratorMin = min(config_->optimizeIntegratorMin, config_->optimizeIntegratorMax) * 0.5f;
        float integratorMax = max(config_->optimizeIntegratorMin, config_->optimizeIntegratorMax) * 0.5f;

        if (optimize_neutral_hold_) {
          // Decay integrator while holding neutral to prevent hidden windup.
          if (optimize_integrator_ > integratorStep) {
            optimize_integrator_ -= integratorStep;
          } else if (optimize_integrator_ < -integratorStep) {
            optimize_integrator_ += integratorStep;
          } else {
            optimize_integrator_ = 0.0f;
          }
        } else {
          float deadband = max(0.0f, config_->optimizeErrorDeadband);
          if (fabs(errorW) > deadband) {
            if (errorW > 0.0f) {
              optimize_integrator_ += integratorStep;
            } else {
              optimize_integrator_ -= integratorStep;
            }
          }

          // Reduce integrator quickly on large mismatch or reversal to avoid fighting battery control.
          float largeErr = max(1.0f, config_->optimizeLargeErrorThreshold);
          if (fabs(errorW) > largeErr || ((errorW * optimize_last_error_w_) < 0.0f)) {
            float reduction = constrain(config_->optimizeIntegratorReduction, 0.0f, 1.0f);
            optimize_integrator_ *= reduction;
          }
        }
        optimize_integrator_ = constrain(optimize_integrator_, integratorMin, integratorMax);
        optimize_last_error_w_ = errorW;

        // Proportional correction outside neutral hold; zero correction while holding neutral.
        float commandOffsetW = 0.0f;
        if (!optimize_neutral_hold_) {
          commandOffsetW = (errorW / adjustDivisor) + optimize_integrator_;

          // Optional stronger correction when error is large.
          if (fabs(errorW) >= max(1.0f, config_->optimizeMinDeliveryForAdjust)) {
            commandOffsetW = errorW + optimize_integrator_;
          }

          // Re-apply configured hysteresis adjustment as directional bias when far from target.
          if (errorW < (tolLow - hysteresisMargin)) {
            commandOffsetW += config_->optimizeHysteresisAdjustment;
          } else if (errorW > (tolHigh + hysteresisMargin)) {
            commandOffsetW -= config_->optimizeHysteresisAdjustment;
          }
        }

        // Build target in scaled domain so filtering is performed on divided value.
        float targetScaledPower = scaledCurrentP1Power + commandOffsetW;
        float targetModifiedPower = targetScaledPower * adjustDivisor;

        // First-order low-pass on modified-power target, updated per telegram interval.
        float tauSec = max(1.0f, config_->optimizeFilterTimeConstant);
        float alpha = dtSec / (tauSec + dtSec);

        if (!filter_initialized_) {
          filtered_delivery_w_ = scaledCurrentP1Power;
          filter_initialized_ = true;
        } else {
          filtered_delivery_w_ += alpha * (targetScaledPower - filtered_delivery_w_);
        }

        newPowerWatt[0] = filtered_delivery_w_ * adjustDivisor;

        // If real and modified signs oppose (beyond deadband), settle to zero to avoid oscillation.
        bool signMismatch =
            (fabs(currentP1Power) > realDeadbandW) &&
            (fabs(newPowerWatt[0]) > realDeadbandW) &&
            ((currentP1Power * newPowerWatt[0]) < 0.0f);

        // If real (non-divided) actual is within deadband, force modified output to zero.
        if (realInDeadband || signMismatch) {
          // Correct low-pass filter state as well, so no stale filtered value remains.
          filtered_delivery_w_ = 0.0f;
          filter_initialized_ = false;
          targetScaledPower = 0.0f;
          targetModifiedPower = 0.0f;
          commandOffsetW = 0.0f;
          newPowerWatt[0] = 0.0f;
          optimize_neutral_hold_ = true;
          optimize_integrator_ *= 0.9f;
        }

        // Output limiter to prevent aggressive battery oscillation:
        // - Normal range: max ±100W
        // - Only when actual exceeds ±1500W: max ±500W
        float outputLimitW = (fabs(currentP1Power) > 1000.0f) ? 500.0f : 100.0f;
        newPowerWatt[0] = constrain(newPowerWatt[0], -outputLimitW, outputLimitW);

        // Save trace values for API/debugging.
        optimize_trace_actual_w_ = currentP1Power;
        optimize_trace_scaled_actual_w_ = scaledCurrentP1Power;
        optimize_trace_error_w_ = errorW;
        optimize_trace_command_w_ = commandOffsetW * adjustDivisor;
        optimize_trace_target_w_ = targetModifiedPower;
        optimize_trace_filtered_w_ = newPowerWatt[0];
        optimize_trace_integrator_w_ = optimize_integrator_;
        optimize_trace_adjust_divisor_ = adjustDivisor;

        logPrint("[OPTIMIZE] Current=");
        logPrint(String(currentP1Power, 1));
        logPrint("W, Scaled=");
        logPrint(String(scaledCurrentP1Power, 1));
        logPrint("W, Error=");
        logPrint(String(errorW, 1));
        logPrint("W, TargetMod=");
        logPrint(String(targetModifiedPower, 1));
        logPrint("W, FilteredMod=");
        logPrint(String(newPowerWatt[0], 1));
        logPrint("W, Setpoint=");
        logPrint(String(optimize_setpoint_, 1));
        logPrint("W, Hold=");
        logPrint(optimize_neutral_hold_ ? "Y" : "N");
        logPrint(", SignMismatch=");
        logPrint(signMismatch ? "Y" : "N");
        logPrint(", I=");
        logPrint(String(optimize_integrator_, 2));
        logPrint("W, Tau=");
        logPrint(String(tauSec, 1));
        logPrintln("s");
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

  if ((fabs(newPowerWatt[0] - static_cast<float>(last_power_watt_)) < 0.01f) && (abs(newPowerWatt[0]) >0.99f)) {
    // Ensure power alternates when it repeats, to avoid sending identical output values.
    newPowerWatt[0] += anti_repeat_add_positive_ ? 1.0f : -1.0f;
    anti_repeat_add_positive_ = !anti_repeat_add_positive_;
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