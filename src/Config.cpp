/*
 * Config.cpp - Implementation of configuration management
 */

#include "Config.h"

Config::Config() {
  reset();
}

void Config::reset() {
  operationMode = MODE_UNMODIFIED;
  batteryPhase = 1;
  modifyPhase = 1;
  forcePower = 3000.0;
  batterySOC = 0.0;
  batteryPower = 0.0;
}

void Config::load(Preferences& prefs) {
  operationMode = (OperationMode)prefs.getInt("opMode", MODE_UNMODIFIED);
  batteryPhase = prefs.getInt("battPhase", 1);
  modifyPhase = prefs.getInt("modPhase", 1);
  forcePower = prefs.getFloat("forcePower", 3000.0);
  
  Serial.println("Configuration loaded from NVS");
  Serial.printf("  Mode: %d\n", operationMode);
  Serial.printf("  Battery Phase: %d\n", batteryPhase);
  Serial.printf("  Modify Phase: %d\n", modifyPhase);
  Serial.printf("  Force Power: %.1f W\n", forcePower);
}

void Config::save(Preferences& prefs) {
  prefs.putInt("opMode", operationMode);
  prefs.putInt("battPhase", batteryPhase);
  prefs.putInt("modPhase", modifyPhase);
  prefs.putFloat("forcePower", forcePower);
  
  Serial.println("Configuration saved to NVS");
}
