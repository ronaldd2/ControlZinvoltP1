/*
 * Config.h - Configuration management for ControlZinvoltP1
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include "P1Modifier.h"

class Config {
public:
  Config();
  
  // Configuration parameters
  OperationMode operationMode;
  int batteryPhase;
  int modifyPhase;
  float forcePower;
  
  // Battery status (from external API - future)
  float batterySOC;
  float batteryPower;
  
  // Load configuration from NVS
  void load(Preferences& prefs);
  
  // Save configuration to NVS
  void save(Preferences& prefs);
  
  // Reset to defaults
  void reset();
};

#endif // CONFIG_H
