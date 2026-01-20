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
  MODE_OFF,               // Prevent charging/discharging (show zero power)
  MODE_FORCE_CHARGE,      // Force charging (show high consumption)
  MODE_FORCE_DISCHARGE,   // Force discharging (show high generation)
  MODE_CHARGE_ONLY,       // Only allow charging (future)
  MODE_DISCHARGE_ONLY     // Only allow discharging (future)
};

class P1Modifier {
public:
  P1Modifier();
  
  // Modify a P1 telegram based on current mode
  String modify(const String& originalTelegram, const P1Parser& parser);
  
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
  
private:
  OperationMode currentMode;
  int batteryPhase;      // Phase where battery is connected (1, 2, or 3)
  int modifyPhase;       // Phase to modify power on (1, 2, or 3)
  float forcePower;      // Power value for force modes (Watts)
  
  // Helper functions
  String modifyObisValue(const String& telegram, const String& obisCode, float newValue);
  String replaceObisValue(const String& telegram, const String& obisCode, const String& newValue);
  String formatPowerValue(float watts);
  String recalculateCRC(const String& telegram);
};

#endif // P1MODIFIER_H
