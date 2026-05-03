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
  singlePhaseMeterMode = false;
  forcePower = 2000.0;
  powerSetpoint = 0.0;   // Default 0W (neutral)
  selfUseLimitThreshold = 20.0;   // Default 20W extra export
  selfUseSmoothingFactor = 0.3;   // Default moderate smoothing
  
  // Optimize mode defaults
  optimizeDeliverySetpoint = 20.0;
  optimizeMinEvaActivity = 10.0;
  optimizeMinSolarPower = 20.0;
  optimizeSolarThreshold = 300.0;
  optimizeHighSolarSetpoint = 100.0;
  optimizeFilterTimeConstant = 45.0;
  optimizeMinDeliveryForAdjust = 60.0;
  optimizeAdjustDivisor = 3.0;
  optimizeToleranceLow = -5.0;
  optimizeToleranceHigh = 15.0;
  optimizeLargeErrorThreshold = 200.0;
  optimizeIntegratorReduction = 0.66;
  optimizeHysteresisDelivery = 40.0;
  optimizeHysteresisAdjustment = -50.0;
  optimizeIntegratorMin = -10.0;
  optimizeIntegratorMax = 10.0;
  optimizeIntegratorStep = 1.0;
  optimizeErrorDeadband = 5.0;
  
  batterySOC = 0.0;
  batteryPower = 0.0;
  batteryCapacity = 0.0;
  batteryProduction = 0.0;
  batteryConsumption = 0.0;
  actualSolarPower = 0.0;
  batteryApiSolarPower = 0.0;
  batteryApiCocPower = 0.0;
  batteryApiMeterPower = 0.0;
  modifiedPowerL1 = 0.0;
  modifiedPowerL2 = 0.0;
  modifiedPowerL3 = 0.0;
  totalModifiedPower = 0.0;
  actualPowerL1 = 0.0;
  actualPowerL2 = 0.0;
  actualPowerL3 = 0.0;
  actualTotalPower = 0.0;
  mqttServer = "";
  mqttPort = 1883;
  mqttUser = "";
  mqttPassword = "";
  evaEnabled = false;
  batteryBackend = "alphaess";
  evaSerialNumber = "";
  evaAppId = "";
  evaAppSecret = "";
  zinvoltEmail = "";
  zinvoltPassword = "";
  zinvoltBatteryId = "";
  useTxReq = false;  // Default: don't check TXREQ (compatible with most devices)
  webUsername = "admin";
  webPassword = "admin";

  // Daily energy tracking defaults
  lastEnergyImport = 0.0f;
  lastEnergyExport = 0.0f;
  dayStartEnergyImport = 0.0f;
  dayStartEnergyExport = 0.0f;
  dayStartDate = "";
  lastTimestamp = "";
  lastTelegram = "";
}

void Config::load(Preferences& prefs) {
  // Load settings only - NOT real-time data
  
  operationMode = (OperationMode)prefs.getInt("opMode", MODE_UNMODIFIED);
  batteryPhase = prefs.getInt("battPhase", 1);
  modifyPhase = prefs.getInt("modPhase", 1);
  singlePhaseMeterMode = prefs.getBool("singlePhase", false);
  forcePower = prefs.getFloat("forcePower", 3000.0);
  powerSetpoint = prefs.getFloat("powerSetpoint", 0.0);
  selfUseLimitThreshold = prefs.getFloat("selfUseThresh", 20.0);
  selfUseSmoothingFactor = prefs.getFloat("selfUseSmooth", 0.3);
  
  // Optimize mode parameters
  optimizeDeliverySetpoint = prefs.getFloat("optDelSetpt", 20.0);
  optimizeMinEvaActivity = prefs.getFloat("optMinEva", 10.0);
  optimizeMinSolarPower = prefs.getFloat("optMinSolar", 20.0);
  optimizeSolarThreshold = prefs.getFloat("optSolarThr", 300.0);
  optimizeHighSolarSetpoint = prefs.getFloat("optHiSolSet", 100.0);
  optimizeFilterTimeConstant = prefs.getFloat("optFiltTau", 45.0);
  optimizeMinDeliveryForAdjust = prefs.getFloat("optMinDelAdj", 60.0);
  optimizeAdjustDivisor = prefs.getFloat("optAdjDiv", 3.0);
  optimizeToleranceLow = prefs.getFloat("optTolLow", -5.0);
  optimizeToleranceHigh = prefs.getFloat("optTolHigh", 15.0);
  optimizeLargeErrorThreshold = prefs.getFloat("optLrgErrThr", 200.0);
  optimizeIntegratorReduction = prefs.getFloat("optIntRed", 0.66);
  optimizeHysteresisDelivery = prefs.getFloat("optHysDel", 40.0);
  optimizeHysteresisAdjustment = prefs.getFloat("optHysAdj", -50.0);
  optimizeIntegratorMin = prefs.getFloat("optIntMin", -10.0);
  optimizeIntegratorMax = prefs.getFloat("optIntMax", 10.0);
  optimizeIntegratorStep = prefs.getFloat("optIntStep", 1.0);
  optimizeErrorDeadband = prefs.getFloat("optErrDead", 5.0);
  
  mqttServer = prefs.getString("mqttServer", "");
  mqttPort = prefs.getInt("mqttPort", 1883);
  mqttUser = prefs.getString("mqttUser", "");
  mqttPassword = prefs.getString("mqttPass", "");
  evaEnabled = prefs.getBool("evaEnabled", false);
  batteryBackend = prefs.getString("batBackend", "alphaess");
  batteryBackend.toLowerCase();
  if (batteryBackend != "alphaess" && batteryBackend != "zinvolt") {
    batteryBackend = "alphaess";
  }
  evaSerialNumber = prefs.getString("evaSN", "");
  evaAppId = prefs.getString("evaAppId", "");
  evaAppSecret = prefs.getString("evaSecret", "");
  zinvoltEmail = prefs.getString("zinEmail", "");
  zinvoltPassword = prefs.getString("zinPass", "");
  zinvoltBatteryId = prefs.getString("zinBattId", "");
  useTxReq = prefs.getBool("useTxReq", false);
  webUsername = prefs.getString("webUser", "admin");
  webPassword = prefs.getString("webPass", "admin");

  // Initialize real-time P1/API data to defaults (not loaded from NVS on startup)
  batterySOC = 0.0f;
  batteryPower = 0.0f;
  batteryCapacity = 0.0f;
  batteryProduction = 0.0f;
  batteryConsumption = 0.0f;
  batteryApiSolarPower = 0.0f;
  batteryApiCocPower = 0.0f;
  batteryApiMeterPower = 0.0f;
  lastEnergyImport = 0.0f;
  lastEnergyExport = 0.0f;
  modifiedPowerL1 = 0.0f;
  modifiedPowerL2 = 0.0f;
  modifiedPowerL3 = 0.0f;
  totalModifiedPower = 0.0f;
  actualPowerL1 = 0.0f;
  actualPowerL2 = 0.0f;
  actualPowerL3 = 0.0f;
  actualTotalPower = 0.0f;
  
    // Load daily baseline (persisted once per day)
    dayStartDate = prefs.getString("dayStartDate", "");
    dayStartEnergyImport = prefs.getFloat("dayStartImport", 0.0f);
    dayStartEnergyExport = prefs.getFloat("dayStartExport", 0.0f);
  lastTimestamp = "";
  lastTelegram = "";
  
  Serial.println("Configuration loaded from NVS");
  Serial.printf("  Mode: %d\n", operationMode);
  Serial.printf("  Battery Phase: %d\n", batteryPhase);
  Serial.printf("  Modify Phase: %d\n", modifyPhase);
  Serial.printf("  Single Phase Meter Mode: %s\n", singlePhaseMeterMode ? "Yes" : "No");
  Serial.printf("  Force Power: %.1f W\n", forcePower);
  Serial.printf("  MQTT Server: %s:%d\n", mqttServer.c_str(), mqttPort);
  Serial.printf("  Use TXREQ: %s\n", useTxReq ? "Yes" : "No");
  if (!dayStartDate.isEmpty()) {
    Serial.printf("  Day Start Date: %s (Import: %.3f kWh, Export: %.3f kWh)\n", 
                  dayStartDate.c_str(), dayStartEnergyImport, dayStartEnergyExport);
  }
}

void Config::save(Preferences& prefs) {
  // Only save settings - NOT real-time data
  
  // Operation settings
  prefs.putInt("opMode", operationMode);
  prefs.putInt("battPhase", batteryPhase);
  prefs.putInt("modPhase", modifyPhase);
  prefs.putBool("singlePhase", singlePhaseMeterMode);
  prefs.putFloat("forcePower", forcePower);
  prefs.putFloat("powerSetpoint", powerSetpoint);
  
  // Self-use limiter settings
  prefs.putFloat("selfUseThresh", selfUseLimitThreshold);
  prefs.putFloat("selfUseSmooth", selfUseSmoothingFactor);
  
  // Optimize mode settings
  prefs.putFloat("optDelSetpt", optimizeDeliverySetpoint);
  prefs.putFloat("optMinEva", optimizeMinEvaActivity);
  prefs.putFloat("optMinSolar", optimizeMinSolarPower);
  prefs.putFloat("optSolarThr", optimizeSolarThreshold);
  prefs.putFloat("optHiSolSet", optimizeHighSolarSetpoint);
  prefs.putFloat("optFiltTau", optimizeFilterTimeConstant);
  prefs.putFloat("optMinDelAdj", optimizeMinDeliveryForAdjust);
  prefs.putFloat("optAdjDiv", optimizeAdjustDivisor);
  prefs.putFloat("optTolLow", optimizeToleranceLow);
  prefs.putFloat("optTolHigh", optimizeToleranceHigh);
  prefs.putFloat("optLrgErrThr", optimizeLargeErrorThreshold);
  prefs.putFloat("optIntRed", optimizeIntegratorReduction);
  prefs.putFloat("optHysDel", optimizeHysteresisDelivery);
  prefs.putFloat("optHysAdj", optimizeHysteresisAdjustment);
  prefs.putFloat("optIntMin", optimizeIntegratorMin);
  prefs.putFloat("optIntMax", optimizeIntegratorMax);
  prefs.putFloat("optIntStep", optimizeIntegratorStep);
  prefs.putFloat("optErrDead", optimizeErrorDeadband);
  
  // MQTT Configuration
  prefs.putString("mqttServer", mqttServer);
  prefs.putInt("mqttPort", mqttPort);
  prefs.putString("mqttUser", mqttUser);
  prefs.putString("mqttPass", mqttPassword);
  
  // AlphaESS EVA Battery API Configuration
  prefs.putBool("evaEnabled", evaEnabled);
  prefs.putString("batBackend", batteryBackend);
  prefs.putString("evaSN", evaSerialNumber);
  prefs.putString("evaAppId", evaAppId);
  prefs.putString("evaSecret", evaAppSecret);
  prefs.putString("zinEmail", zinvoltEmail);
  prefs.putString("zinPass", zinvoltPassword);
  prefs.putString("zinBattId", zinvoltBatteryId);
  
  // Hardware Configuration
  prefs.putBool("useTxReq", useTxReq);
  
  // Web Interface Authentication
  prefs.putString("webUser", webUsername);
  prefs.putString("webPass", webPassword);
  
  // Persist daily baseline data (once per day when date changes)
  prefs.putString("dayStartDate", dayStartDate);
  prefs.putFloat("dayStartImport", dayStartEnergyImport);
  prefs.putFloat("dayStartExport", dayStartEnergyExport);
  
  // NOTE: NOT persisting real-time data to avoid excessive NVS writes:
  // - batterySOC, batteryPower, batteryCapacity, batteryProduction, batteryConsumption (from API)
  // - lastEnergyImport, lastEnergyExport, modifiedPowerL*, actualPowerL*, totalModifiedPower (P1 data)
  // - lastTimestamp, lastTelegram (P1 telegram data)
  
  Serial.println("Configuration saved to NVS");
}
