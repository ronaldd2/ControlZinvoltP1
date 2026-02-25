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
  float powerSetpoint;  // Power setpoint for MODE_POWER_CONTROL (Watts)
  
  // Self-use limiter settings
  float selfUseLimitThreshold;   // Extra W to deliver to grid in SELF_USE_LIMITER mode (default 20W)
  float selfUseSmoothingFactor;  // Smoothing factor 0.1-1.0 (lower = more smoothing)
  
  // Optimize mode (Domoticz control algorithm) settings
  float optimizeDeliverySetpoint;        // Target grid delivery in W (default 20W)
  float optimizeMinEvaActivity;          // Min EVA charge/discharge to consider active (default 10W)
  float optimizeMinSolarPower;           // Min solar power threshold (default 20W)
  float optimizeSolarThreshold;          // High solar power threshold (default 300W)
  float optimizeHighSolarSetpoint;       // Setpoint when solar is high (default 100W)
  float optimizeMinDeliveryForAdjust;    // Min delivery for full adjustment (default 60W)
  float optimizeAdjustDivisor;           // Divisor for slow adjustment (default 3.0)
  float optimizeToleranceLow;            // Lower tolerance band (default -5W)
  float optimizeToleranceHigh;           // Upper tolerance band (default 15W)
  float optimizeLargeErrorThreshold;     // Error threshold for integrator reduction (default 200W)
  float optimizeIntegratorReduction;     // Integrator reduction factor (default 0.66)
  float optimizeHysteresisDelivery;      // Delivery threshold for hysteresis (default 40W)
  float optimizeHysteresisAdjustment;    // Hysteresis adjustment value (default -50W)
  float optimizeIntegratorMin;           // Integrator min clamp (default -10.0)
  float optimizeIntegratorMax;           // Integrator max clamp (default 10.0)
  float optimizeIntegratorStep;          // Integrator increment step (default 1.0)
  float optimizeErrorDeadband;           // Error deadband for integrator update (default 5W)
  
  // Battery status (from external API - future)
  float batterySOC;
  float batteryPower;
  float gridPower;
  float batteryCapacity;
  float batteryProduction;
  float batteryConsumption;
  
  // Solar power (from MQTT input)
  float actualSolarPower;
  
  // Modified telegram power values (for display)
  float modifiedPowerL1;
  float modifiedPowerL2;
  float modifiedPowerL3;
  float totalModifiedPower;

  // Snapshot of last parsed actual power values (kept in sync with modified snapshot)
  float actualPowerL1;
  float actualPowerL2;
  float actualPowerL3;
  float actualTotalPower;
  
  // MQTT Configuration
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPassword;
  
  // AlphaESS EVA Battery API Configuration
  bool evaEnabled;
  String evaSerialNumber;
  String evaAppId;
  String evaAppSecret;
  
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
