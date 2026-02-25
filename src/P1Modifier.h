/*
 * P1Modifier.h - Modifies P1 telegrams based on operation mode
 * 
 * Handles different operation modes for battery control
 */

#ifndef P1MODIFIER_H
#define P1MODIFIER_H

#include <Arduino.h>
#include "P1Parser.h"
#include "DomoticzLogic.h"

enum OperationMode {
  MODE_UNMODIFIED,        // Forward without modification
  MODE_BATTERY_OFF,               // Prevent charging/discharging (gradual power reduction)
  MODE_FORCE_CHARGE,      // Force charging (show high consumption)
  MODE_FORCE_DISCHARGE,   // Force discharging (show high generation)
  MODE_POWER_CONTROL,     // Control to specific power setpoint (+ or -)
  MODE_CHARGE_ONLY,       // Only allow charging (gradual discharge reduction)
  MODE_DISCHARGE_ONLY,    // Only allow discharging (gradual charge reduction)
  MODE_EXTERNAL_CONTROL,  // External REST API control
  MODE_OPTIMIZE   // Limit self-use: prioritize grid export with smoothing
};

enum BatteryMode { 
  BM_OFF,
  BM_CHARGING,
  BM_DISCHARGING
} ; 


class P1Modifier {
public:
  P1Modifier();
  
  // Set config pointer for accessing external data
  void setConfig(class Config* config) { config_ = config; }
  
  // Modify a P1 telegram based on current mode
  String modify(const String& originalTelegram, const P1Parser& parser, float batteryPower = 0);

  
  // Mode management
  void setMode(OperationMode mode) { current_mode_ = mode; }
  OperationMode getMode() const { return current_mode_; }
  String getModeString() const;
  
  // Phase configuration
  void setBatteryPhase(int phase) { battery_phase_ = phase; }
  int getBatteryPhase() const { return battery_phase_; }
  
  void setModifyPhase(int phase) { modify_phase_ = phase; }
  int getModifyPhase() const { return modify_phase_; }
  
  // Force power settings (in Watts)
  void setForcePower(float watts) { force_power_ = watts; }
  float getForcePower() const { return force_power_; }
  
  // Power control setpoint (in Watts, + for discharge, - for charge)
  void setPowerSetpoint(float watts) { power_setpoint_ = watts; }
  float getPowerSetpoint() const { return power_setpoint_; }
  
  // External control settings
  void setExternalControlPower(float watts) { external_control_power_ = watts; external_control_last_update_ = millis(); }
  float getExternalControlPower() const { return external_control_power_; }
  unsigned long getExternalControlLastUpdate() const { return external_control_last_update_; }
  bool isExternalControlValid() const { return (millis() - external_control_last_update_) < 60000; }
    // Get detected telegram interval in seconds
  float getTelegramInterval() const { return telegram_interval_sec_; }
    // Self-use limiter settings
  void setSelfUseLimitThreshold(float watts) { self_use_limit_threshold_ = watts; }
  float getSelfUseLimitThreshold() const { return self_use_limit_threshold_; }
  void setSelfUseLimitSmoothing(float factor) { self_use_smoothing_factor_ = constrain(factor, 0.1f, 1.0f); }
  float getSelfUseLimitSmoothing() const { return self_use_smoothing_factor_; }
  
private:
  OperationMode current_mode_;
  int battery_phase_;      // Phase where battery is connected (1, 2, or 3)
  int modify_phase_;       // Phase to modify power on (1, 2, or 3)
  float force_power_;      // Power value for force modes (Watts)
  float power_setpoint_;   // Power setpoint for MODE_POWER_CONTROL (Watts, + or -)
  float external_control_power_;  // Power value from external REST API (Watts)
  unsigned long external_control_last_update_;  // Timestamp of last external control update
  int8_t noise_;        // Noise value for power variation
  float force_integrator_;
  float power_adjustment_;
  int last_power_watt_;  // Last adjusted power for noise calculation

  
  // Self-use limiter smoothing
  float self_use_limit_threshold_;     // Extra power to deliver to grid (default 20W)
  float self_use_smoothing_factor_;    // Smoothing factor 0.1-1.0 (lower = more smoothing)
  float last_smoothed_power_;         // Last smoothed power value for hysteresis
  unsigned long last_smooth_update_time_; // Timestamp of last smoothing update
  float optimize_setpoint_;         // Current optimize setpoint
  
  // Lag-aware charge/discharge mode tracking
  float previous_battery_power_;      // Previous battery power reading to detect trends
  unsigned long last_battery_update_; // When battery power was last updated
  float current_direction_;          // Battery power direction trend: negative=charging, positive=discharging
  unsigned long last_direction_change_; // When direction last changed to detect reversals
  BatteryMode battery_mode_;        // Current battery mode based on trend
  
  // Telegram interval tracking
  unsigned long last_modify_time_;   // Timestamp of last modify() call
  float telegram_interval_sec_;      // Detected telegram interval in seconds
  
  // Domoticz logic for optimize mode
  DomoticzLogic domoticz_logic_;
  class Config* config_;  // Pointer to config for accessing solar/battery data
  
  // Helper functions
  String modifyObisValue(const String& telegram, const String& obisCode, float newValue);
  String modifyObisPhase(const String& originalTelegram, uint8_t phase, float newPowerWatt); 
  String replaceObisValue(const String& telegram, const String& obisCode, const String& newValue);
  String formatPowerValue(float watts);
  String recalculateCRC(const String& telegram);
  float getPowerDirection(float batteryPower);  // Returns trend: negative/positive/zero
  // Generic integrator updater with optional symmetric step and guards
  float updateIntegrator(float currentValue, float step, bool applyPos, bool applyNeg);
};

#endif // P1MODIFIER_H
