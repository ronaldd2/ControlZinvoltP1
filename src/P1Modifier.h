/*
 * P1Modifier.h - Modifies P1 telegrams based on operation mode
 * 
 * Handles different operation modes for battery control
 */

#ifndef P1MODIFIER_H
#define P1MODIFIER_H

#include <Arduino.h>
#include "P1Parser.h"

enum OperationMode {
  MODE_UNMODIFIED=0,        // Forward without modification
  MODE_BATTERY_OFF=1,               // Prevent charging/discharging (gradual power reduction)
  MODE_FORCE_CHARGE=2,      // Force charging (show high consumption)
  MODE_FORCE_DISCHARGE=3,   // Force discharging (show high generation)
  MODE_POWER_CONTROL=4,     // Control to specific power setpoint (+ or -)
  MODE_CHARGE_ONLY=5,       // Only allow charging (gradual discharge reduction)
  MODE_DISCHARGE_ONLY=6,    // Only allow discharging (gradual charge reduction)
  MODE_EXTERNAL_CONTROL=7,  // External REST API control
  MODE_OPTIMIZE=8   // Limit self-use: prioritize grid export with smoothing
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

  void setSinglePhaseMeterMode(bool enabled) { single_phase_meter_mode_ = enabled; }
  bool getSinglePhaseMeterMode() const { return single_phase_meter_mode_; }
  
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
  float getOptimizeSetpoint() const { return optimize_setpoint_; }
  void calculateOptimizeSetpoint();
  
private:
  OperationMode current_mode_;
  int battery_phase_;      // Phase where battery is connected (1, 2, or 3)
  int modify_phase_;       // Phase to modify power on (1, 2, or 3)
  bool single_phase_meter_mode_;  // If enabled, force L2/L3 to zero and map total to L1
  float force_power_;      // Power value for force modes (Watts)
  float power_setpoint_;   // Power setpoint for MODE_POWER_CONTROL (Watts, + or -)
  float external_control_power_;  // Power value from external REST API (Watts)
  unsigned long external_control_last_update_;  // Timestamp of last external control update
  float force_integrator_;
  int last_power_watt_;  // Last adjusted power for noise calculation
  bool anti_repeat_add_positive_; // Toggle for alternating +1/-1 when output repeats

  // Self-use limiter
  float self_use_limit_threshold_;     // Extra power to deliver to grid (default 20W)
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

  class Config* config_;  // Pointer to config for accessing solar/battery data
  
  // Helper functions
  String modifyObisValue(const String& telegram, const String& obisCode, float newValue);
  String modifyObisPhase(const String& originalTelegram, uint8_t phase, float newPowerWatt); 
  String formatPowerValue(float watts);
  String recalculateCRC(const String& telegram);
  float getPowerDirection(float batteryPower);  // Returns trend: negative/positive/zero
  // Generic integrator updater with optional symmetric step and guards
  float updateIntegrator(float currentValue, float step, bool applyPos, bool applyNeg);
  // Shared optimize adjustment used by optimize/charge-only/discharge-only modes.
  float calculateOptimizeAdjustmentW(float activePowerW);
};

#endif // P1MODIFIER_H
