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
  forcePower = 2000.0;
  batterySOC = 0.0;
  batteryPower = 0.0;
  batteryCapacity = 0.0;
  batteryProduction = 0.0;
  batteryConsumption = 0.0;
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
  evaSerialNumber = "";
  evaAppId = "";
  evaAppSecret = "";
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
  operationMode = (OperationMode)prefs.getInt("opMode", MODE_UNMODIFIED);
  batteryPhase = prefs.getInt("battPhase", 1);
  modifyPhase = prefs.getInt("modPhase", 1);
  forcePower = prefs.getFloat("forcePower", 3000.0);
  mqttServer = prefs.getString("mqttServer", "");
  mqttPort = prefs.getInt("mqttPort", 1883);
  mqttUser = prefs.getString("mqttUser", "");
  mqttPassword = prefs.getString("mqttPass", "");
  evaEnabled = prefs.getBool("evaEnabled", false);
  evaSerialNumber = prefs.getString("evaSN", "");
  evaAppId = prefs.getString("evaAppId", "");
  evaAppSecret = prefs.getString("evaSecret", "");
  useTxReq = prefs.getBool("useTxReq", false);
  webUsername = prefs.getString("webUser", "admin");
  webPassword = prefs.getString("webPass", "admin");

  // Battery values
  batterySOC = prefs.getFloat("battSOC", 0.0f);
  batteryPower = prefs.getFloat("battPower", 0.0f);
  batteryCapacity = prefs.getFloat("battCap", 0.0f);
  batteryProduction = prefs.getFloat("battProd", 0.0f);
  batteryConsumption = prefs.getFloat("battCons", 0.0f);

  // Daily energy tracking
  lastEnergyImport = prefs.getFloat("lastImp", 0.0f);
  lastEnergyExport = prefs.getFloat("lastExp", 0.0f);
  dayStartEnergyImport = prefs.getFloat("dayImp", 0.0f);
  dayStartEnergyExport = prefs.getFloat("dayExp", 0.0f);
  dayStartDate = prefs.getString("dayDate", "");
  lastTimestamp = prefs.getString("lastTs", "");
  lastTelegram = prefs.getString("lastTlg", "");
  
  Serial.println("Configuration loaded from NVS");
  Serial.printf("  Mode: %d\n", operationMode);
  Serial.printf("  Battery Phase: %d\n", batteryPhase);
  Serial.printf("  Modify Phase: %d\n", modifyPhase);
  Serial.printf("  Force Power: %.1f W\n", forcePower);
  Serial.printf("  MQTT Server: %s:%d\n", mqttServer.c_str(), mqttPort);
  Serial.printf("  Use TXREQ: %s\n", useTxReq ? "Yes" : "No");
}

void Config::save(Preferences& prefs) {
  prefs.putInt("opMode", operationMode);
  prefs.putInt("battPhase", batteryPhase);
  prefs.putInt("modPhase", modifyPhase);
  prefs.putFloat("forcePower", forcePower);
  prefs.putString("mqttServer", mqttServer);
  prefs.putInt("mqttPort", mqttPort);
  prefs.putString("mqttUser", mqttUser);
  prefs.putString("mqttPass", mqttPassword);
  prefs.putBool("evaEnabled", evaEnabled);
  prefs.putString("evaSN", evaSerialNumber);
  prefs.putString("evaAppId", evaAppId);
  prefs.putString("evaSecret", evaAppSecret);
  prefs.putBool("useTxReq", useTxReq);
  prefs.putString("webUser", webUsername);
  prefs.putString("webPass", webPassword);

  // Battery values (optional; persisted for reuse)
  prefs.putFloat("battSOC", batterySOC);
  prefs.putFloat("battPower", batteryPower);
  prefs.putFloat("battCap", batteryCapacity);
  prefs.putFloat("battProd", batteryProduction);
  prefs.putFloat("battCons", batteryConsumption);

  // Daily energy tracking
  prefs.putFloat("lastImp", lastEnergyImport);
  prefs.putFloat("lastExp", lastEnergyExport);
  prefs.putFloat("dayImp", dayStartEnergyImport);
  prefs.putFloat("dayExp", dayStartEnergyExport);
  prefs.putString("dayDate", dayStartDate);
  prefs.putString("lastTs", lastTimestamp);
  prefs.putString("lastTlg", lastTelegram);
  
  Serial.println("Configuration saved to NVS");
}
