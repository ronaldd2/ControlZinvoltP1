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
  MODE_UNMODIFIED,        // Forward without modification
  MODE_OFF,               // Prevent charging/discharging (gradual power reduction)
  MODE_FORCE_CHARGE,      // Force charging (show high consumption)
  MODE_FORCE_DISCHARGE,   // Force discharging (show high generation)
  MODE_CHARGE_ONLY,       // Only allow charging (gradual discharge reduction)
  MODE_DISCHARGE_ONLY,    // Only allow discharging (gradual charge reduction)
  MODE_EXTERNAL_CONTROL,  // External REST API control
  MODE_SELF_USE_LIMITER   // Limit self-use: prioritize grid export with smoothing
};

enum BatteryMode { 
  BM_OFF,
  BM_CHARGING,
  BM_DISCHARGING
} ; 


class P1Modifier {
public:
  P1Modifier();
  
  // Modify a P1 telegram based on current mode
  String modify(const String& originalTelegram, const P1Parser& parser, float batteryPower = 0);

  
  // Mode management
  void setMode(OperationMode mode) { currentMode = mode; }
  OperationMode getMode() const { return currentMode; }
  String getModeString() const;
  
  // Phase configuration
  void setBatteryPhase(int phase) { batteryPhase = phase; }
  int getBatteryPhase() const { return batteryPhase; }
  
  void setModifyPhase(int phase) { modifyPhase = phase; }
  int getModifyPhase() const { return modifyPhase; }
  
  // Force power settings (in Watts)
  void setForcePower(float watts) { forcePower = watts; }
  float getForcePower() const { return forcePower; }
  
  // External control settings
  void setExternalControlPower(float watts) { externalControlPower = watts; externalControlLastUpdate = millis(); }
  float getExternalControlPower() const { return externalControlPower; }
  unsigned long getExternalControlLastUpdate() const { return externalControlLastUpdate; }
  bool isExternalControlValid() const { return (millis() - externalControlLastUpdate) < 60000; }
    // Get detected telegram interval in seconds
  float getTelegramInterval() const { return telegramIntervalSec; }
    // Self-use limiter settings
  void setSelfUseLimitThreshold(float watts) { selfUseLimitThreshold = watts; }
  float getSelfUseLimitThreshold() const { return selfUseLimitThreshold; }
  void setSelfUseLimitSmoothing(float factor) { selfUseSmoothingFactor = constrain(factor, 0.1f, 1.0f); }
  float getSelfUseLimitSmoothing() const { return selfUseSmoothingFactor; }
  
private:
  OperationMode currentMode;
  int batteryPhase;      // Phase where battery is connected (1, 2, or 3)
  int modifyPhase;       // Phase to modify power on (1, 2, or 3)
  float forcePower;      // Power value for force modes (Watts)
  float externalControlPower;  // Power value from external REST API (Watts)
  unsigned long externalControlLastUpdate;  // Timestamp of last external control update
  int8_t _noise;        // Noise value for power variation
  float _force_integrator;
  float _powerAdjustment;

  
  // Self-use limiter smoothing
  float selfUseLimitThreshold;     // Extra power to deliver to grid (default 20W)
  float selfUseSmoothingFactor;    // Smoothing factor 0.1-1.0 (lower = more smoothing)
  float lastSmoothedPower;         // Last smoothed power value for hysteresis
  unsigned long lastSmoothUpdateTime; // Timestamp of last smoothing update
  
  // Lag-aware charge/discharge mode tracking
  float previousBatteryPower;      // Previous battery power reading to detect trends
  unsigned long lastBatteryUpdate; // When battery power was last updated
  float currentDirection;          // Battery power direction trend: negative=charging, positive=discharging
  unsigned long lastDirectionChange; // When direction last changed to detect reversals
  BatteryMode batteryMode;        // Current battery mode based on trend
  
  // Telegram interval tracking
  unsigned long lastModifyTime;   // Timestamp of last modify() call
  float telegramIntervalSec;      // Detected telegram interval in seconds
  
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
