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
  
  // MQTT Configuration
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPassword;
  
  // Hardware Configuration
  bool useTxReq;  // Enable TXREQ pin check before sending
  
  // Web Interface Authentication
  String webUsername;
  String webPassword;

  // Energy tracking (persisted daily baselines and last readings)
  float lastEnergyImport;
  float lastEnergyExport;
  float dayStartEnergyImport;
  float dayStartEnergyExport;
  String dayStartDate; // YYYYMMDD
  String lastTimestamp;
  String lastTelegram;
  
  // Load configuration from NVS
  void load(Preferences& prefs);
  
  // Save configuration to NVS
  void save(Preferences& prefs);
  
  // Reset to defaults
  void reset();
};

#endif // CONFIG_H
