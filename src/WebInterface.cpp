/*
 * WebInterface.cpp - Implementation of web server and REST API
 */

#include "WebInterface.h"
#include "Version.h"
#include "ActualsPage.h"
#include "SettingsPage.h"
#include "UpdatePage.h"
#include <Preferences.h>

// External references declared in main.cpp
extern Preferences preferences;

WebInterface::WebInterface(AsyncWebServer* server, P1Parser* parser, P1Modifier* modifier, Config* config) {
  server_ = server;
  parser_ = parser;
  modifier_ = modifier;
  config_ = config;
}

void WebInterface::begin() {
    auto checkAuth = [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(config_->webUsername.c_str(), config_->webPassword.c_str())) {
            request->requestAuthentication();
            return false;
        }
        return true;
    };

    server_->on("/", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleActualsPage(request);
    });

    server_->on("/actuals", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleActualsPage(request);
    });

    server_->on("/settings", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleSettingsPage(request);
    });

    server_->on("/update", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleUpdatePage(request);
    });
  
  // REST API endpoints (no authentication required)
  server_->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetStatus(request);
  });
  
  server_->on("/api/p1data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetP1Data(request);
  });
  
  server_->on("/api/mode", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetMode(request);
  });
  
  server_->on("/api/phase", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetPhase(request);
  });

  server_->on("/api/singlephase", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetSinglePhaseMode(request);
  });
  
  server_->on("/api/power", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetPower(request);
  });
  
  server_->on("/api/setpoint", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetPowerSetpoint(request);
  });
  
  server_->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetConfig(request);
  });
  
  server_->on("/api/mqtt", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetMqttConfig(request);
  });
  
  server_->on("/api/mqtt", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetMqttConfig(request);
  });
  
  server_->on("/api/advanced", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetAdvancedConfig(request);
  });
  
  server_->on("/api/advanced", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetAdvancedConfig(request);
  });
  
  server_->on("/api/reboot", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleReboot(request);
  });
  
  server_->on("/api/eva", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetEvaConfig(request);
  });
  
  server_->on("/api/eva", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetEvaConfig(request);
  });
  
  server_->on("/api/optimize", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetOptimizeConfig(request);
  });
  
  server_->on("/api/optimize", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetOptimizeConfig(request);
  });
  
  server_->on("/api/external", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetExternalControl(request);
  });
  
  // 404 handler
  server_->onNotFound([this](AsyncWebServerRequest* request) {
    handleNotFound(request);
  });

  // Manual OTA update handler with authentication and validation
  server_->on("/update", HTTP_POST, 
    [this](AsyncWebServerRequest *request) {
      if (!request->authenticate(config_->webUsername.c_str(), config_->webPassword.c_str())) {
        return request->requestAuthentication();
      }
      
      bool shouldReboot = !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", 
        shouldReboot ? "OK" : "FAIL");
      response->addHeader("Connection", "close");
      request->send(response);
      
      if (shouldReboot) {
        Serial.println("[OTA] Update successful! Rebooting...");
        delay(100);
        ESP.restart();
      }
    },
    [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!request->authenticate(config_->webUsername.c_str(), config_->webPassword.c_str())) {
        return;
      }
      
      if (!index) {
        Serial.printf("[OTA] Update started: %s\n", filename.c_str());
        
        // Validate file extension
        if (!filename.endsWith(".bin")) {
          Serial.println("[OTA] Error: Not a .bin file");
          Update.abort();
          return;
        }
        
        // Start update process
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          Serial.printf("[OTA] Begin failed: %s\n", Update.errorString());
          Update.abort();
          return;
        }
      }
      
      // Write chunk
      if (Update.write(data, len) != len) {
        Serial.printf("[OTA] Write failed: %s\n", Update.errorString());
        Update.abort();
        return;
      }
      
      // Progress feedback
      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 1000) {
        Serial.printf("[OTA] Progress: %u bytes\n", index + len);
        lastPrint = millis();
      }
      
      // Finalize on last chunk
      if (final) {
        if (Update.end(true)) {
          Serial.printf("[OTA] Success: %u bytes\n", index + len);
        } else {
          Serial.printf("[OTA] End failed: %s\n", Update.errorString());
        }
      }
    }
  );
  
  Serial.println("Web interface routes configured");
}

void WebInterface::handleRoot(AsyncWebServerRequest* request) {
    handleActualsPage(request);
}

void WebInterface::handleActualsPage(AsyncWebServerRequest* request) {
    request->send(200, "text/html", getActualsPage());
}

void WebInterface::handleSettingsPage(AsyncWebServerRequest* request) {
    request->send(200, "text/html", getSettingsPage());
}

void WebInterface::handleUpdatePage(AsyncWebServerRequest* request) {
    request->send(200, "text/html", getUpdatePage());
}

void WebInterface::handleGetStatus(AsyncWebServerRequest* request) {
  request->send(200, "application/json", getStatusJSON());
}

void WebInterface::handleGetP1Data(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["valid"] = parser_->isValid();
  doc["timestamp"] = parser_->getTimestamp();
  
  // Power data
  JsonObject power = doc["power"].to<JsonObject>();
    power["total"] = config_->actualTotalPower;
    power["l1"] = config_->actualPowerL1;
    power["l2"] = config_->actualPowerL2;
    power["l3"] = config_->actualPowerL3;

    // Modified power data (kept in sync with last processed telegram)
    JsonObject mod = doc["modifier"].to<JsonObject>();
    mod["total"] = config_->totalModifiedPower;
    mod["l1"] = config_->modifiedPowerL1;
    mod["l2"] = config_->modifiedPowerL2;
    mod["l3"] = config_->modifiedPowerL3;
  
  // Voltage data
  JsonObject voltage = doc["voltage"].to<JsonObject>();
  voltage["l1"] = parser_->getVoltageL1();
  voltage["l2"] = parser_->getVoltageL2();
  voltage["l3"] = parser_->getVoltageL3();
  
  // Current data
  JsonObject current = doc["current"].to<JsonObject>();
  current["l1"] = parser_->getCurrentL1();
  current["l2"] = parser_->getCurrentL2();
  current["l3"] = parser_->getCurrentL3();
  
  // Energy data
  JsonObject energy = doc["energy"].to<JsonObject>();
    float totalImport = parser_->getTotalEnergyImport();
    float totalExport = parser_->getTotalEnergyExport();
    energy["import"] = totalImport;   // lifetime (kWh)
    energy["export"] = totalExport;   // lifetime (kWh)
    // Today's values: (current - baseline at day start)
    float todayImport = totalImport - config_->dayStartEnergyImport;
    float todayExport = totalExport - config_->dayStartEnergyExport;
    if (todayImport < 0) todayImport = 0;
    if (todayExport < 0) todayExport = 0;
    energy["todayImport"] = todayImport;
    energy["todayExport"] = todayExport;
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetMode(AsyncWebServerRequest* request) {
  if (!request->hasParam("value")) {
    request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
    return;
  }
  
  int mode = request->getParam("value")->value().toInt();
  
  if (mode < 0 || mode > 8) {
    request->send(400, "application/json", "{\"error\":\"Invalid mode value (0-8)\"}");
    return;
  }
  
  modifier_->setMode((OperationMode)mode);
  config_->operationMode = (OperationMode)mode;
  config_->save(preferences);
  
  Serial.printf("Mode changed to: %s\n", modifier_->getModeString().c_str());
  
  request->send(200, "application/json", "{\"success\":true,\"mode\":" + String(mode) + "}");
}

void WebInterface::handleSetPhase(AsyncWebServerRequest* request) {
  if (!request->hasParam("battery") && !request->hasParam("modify")) {
    request->send(400, "application/json", "{\"error\":\"Missing battery or modify parameter\"}");
    return;
  }
  
  JsonDocument doc;
  doc["success"] = true;
  
  if (request->hasParam("battery")) {
    int phase = request->getParam("battery")->value().toInt();
    if (phase < 1 || phase > 3) {
      request->send(400, "application/json", "{\"error\":\"Invalid battery phase (1-3)\"}");
      return;
    }
    modifier_->setBatteryPhase(phase);
    config_->batteryPhase = phase;
    config_->save(preferences);
    doc["batteryPhase"] = phase;
    Serial.printf("Battery phase set to: %d\n", phase);
  }
  
  if (request->hasParam("modify")) {
    int phase = request->getParam("modify")->value().toInt();
    if (phase < 1 || phase > 3) {
      request->send(400, "application/json", "{\"error\":\"Invalid modify phase (1-3)\"}");
      return;
    }
    modifier_->setModifyPhase(phase);
    config_->modifyPhase = phase;
    config_->save(preferences);
    doc["modifyPhase"] = phase;
    Serial.printf("Modify phase set to: %d\n", phase);
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetSinglePhaseMode(AsyncWebServerRequest* request) {
  if (!request->hasParam("enabled")) {
    request->send(400, "application/json", "{\"error\":\"Missing enabled parameter\"}");
    return;
  }

  String value = request->getParam("enabled")->value();
  bool enabled = (value == "1" || value == "true" || value == "on");

  modifier_->setSinglePhaseMeterMode(enabled);
  config_->singlePhaseMeterMode = enabled;
  config_->save(preferences);

  Serial.printf("Single phase meter mode: %s\n", enabled ? "Enabled" : "Disabled");

  JsonDocument doc;
  doc["success"] = true;
  doc["singlePhaseMode"] = enabled;

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetPower(AsyncWebServerRequest* request) {
  if (!request->hasParam("value")) {
    request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
    return;
  }
  
  float power = request->getParam("value")->value().toFloat();
  
  if (power < 0 || power > 20000) {
    request->send(400, "application/json", "{\"error\":\"Invalid power value (0-20000 W)\"}");
    return;
  }
  
  modifier_->setForcePower(power);
  config_->forcePower = power;
  config_->save(preferences);
  
  Serial.printf("Force power set to: %.1f W\n", power);
  
  request->send(200, "application/json", "{\"success\":true,\"power\":" + String(power) + "}");
}

void WebInterface::handleSetPowerSetpoint(AsyncWebServerRequest* request) {
  if (!request->hasParam("value")) {
    request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
    return;
  }
  
  float setpoint = request->getParam("value")->value().toFloat();
  
  if (setpoint < -20000 || setpoint > 20000) {
    request->send(400, "application/json", "{\"error\":\"Invalid setpoint value (-20000 to 20000 W)\"}");
    return;
  }
  
  modifier_->setPowerSetpoint(setpoint);
  config_->powerSetpoint = setpoint;
  config_->save(preferences);
  
  Serial.printf("Power setpoint set to: %.1f W\n", setpoint);
  
  request->send(200, "application/json", "{\"success\":true,\"setpoint\":" + String(setpoint) + "}");
}

void WebInterface::handleGetConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["mode"] = modifier_->getMode();
  doc["modeString"] = modifier_->getModeString();
  doc["batteryPhase"] = modifier_->getBatteryPhase();
  doc["modifyPhase"] = modifier_->getModifyPhase();
  doc["singlePhaseMode"] = modifier_->getSinglePhaseMeterMode();
  doc["forcePower"] = modifier_->getForcePower();
  doc["powerSetpoint"] = modifier_->getPowerSetpoint();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleGetMqttConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["server"] = config_->mqttServer;
  doc["port"] = config_->mqttPort;
  doc["user"] = config_->mqttUser;
  // Don't send password for security
  doc["hasPassword"] = !config_->mqttPassword.isEmpty();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetMqttConfig(AsyncWebServerRequest* request) {
  if (!request->hasParam("server", true)) {
    request->send(400, "application/json", "{\"error\":\"Missing server parameter\"}");
    return;
  }
  
  config_->mqttServer = request->getParam("server", true)->value();
  config_->mqttPort = request->hasParam("port", true) ? 
                      request->getParam("port", true)->value().toInt() : 1883;
  
  if (request->hasParam("user", true)) {
    config_->mqttUser = request->getParam("user", true)->value();
  }
  
  if (request->hasParam("password", true)) {
    String pwd = request->getParam("password", true)->value();
    if (!pwd.isEmpty()) {
      config_->mqttPassword = pwd;
    }
  }
  
  // Save to NVS
  config_->save(preferences);
  
  // Trigger MQTT reconnection via callback in main.cpp
  extern void reconnectMqtt();
  reconnectMqtt();

  Serial.println("MQTT configuration updated:");
  Serial.printf("  Server: %s:%d\n", config_->mqttServer.c_str(), config_->mqttPort);
  Serial.printf("  User: %s\n", config_->mqttUser.c_str());
  
  request->send(200, "application/json", 
                "{\"success\":true,\"message\":\"MQTT config saved. Device will reconnect.\"}");
}

void WebInterface::handleGetAdvancedConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["useTxReq"] = config_->useTxReq;
  doc["singlePhaseMode"] = config_->singlePhaseMeterMode;
  doc["webUsername"] = config_->webUsername;
  // Don't send password for security
  doc["hasPassword"] = !config_->webPassword.isEmpty();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetAdvancedConfig(AsyncWebServerRequest* request) {
  if (request->hasParam("useTxReq", true)) {
    String value = request->getParam("useTxReq", true)->value();
    config_->useTxReq = (value == "true" || value == "1");
  }

  if (request->hasParam("singlePhaseMode", true)) {
    String value = request->getParam("singlePhaseMode", true)->value();
    bool enabled = (value == "true" || value == "1");
    config_->singlePhaseMeterMode = enabled;
    modifier_->setSinglePhaseMeterMode(enabled);
  }
  
  if (request->hasParam("webUsername", true)) {
    config_->webUsername = request->getParam("webUsername", true)->value();
  }
  
  if (request->hasParam("webPassword", true)) {
    String pwd = request->getParam("webPassword", true)->value();
    if (!pwd.isEmpty()) {
      config_->webPassword = pwd;
    }
  }
  
  // Save to NVS
  config_->save(preferences);
  
  Serial.println("Advanced configuration updated");
  Serial.printf("  Use TXREQ: %s\n", config_->useTxReq ? "Yes" : "No");
  Serial.printf("  Web Username: %s\n", config_->webUsername.c_str());
  
  request->send(200, "application/json", 
                "{\"success\":true,\"message\":\"Advanced settings saved.\"}");
}

void WebInterface::handleReboot(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["success"] = true;
  doc["message"] = "Device rebooting";

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);

  Serial.println("[WEB] Reboot requested via API");
  delay(200);
  ESP.restart();
}

void WebInterface::handleGetEvaConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["enabled"] = config_->evaEnabled;
  doc["backend"] = config_->batteryBackend;
  doc["serialNumber"] = config_->evaSerialNumber;
  doc["appId"] = config_->evaAppId;
  doc["hasSecret"] = !config_->evaAppSecret.isEmpty();
  doc["zinvoltEmail"] = config_->zinvoltEmail;
  doc["zinvoltBatteryId"] = config_->zinvoltBatteryId;
  doc["hasZinvoltPassword"] = !config_->zinvoltPassword.isEmpty();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetEvaConfig(AsyncWebServerRequest* request) {
  if (request->hasParam("enabled", true)) {
    String value = request->getParam("enabled", true)->value();
    config_->evaEnabled = (value == "true" || value == "1");
  }

  if (request->hasParam("backend", true)) {
    String backend = request->getParam("backend", true)->value();
    backend.toLowerCase();
    if (backend == "zinvolt" || backend == "alphaess") {
      config_->batteryBackend = backend;
    }
  }
  
  if (request->hasParam("serialNumber", true)) {
    config_->evaSerialNumber = request->getParam("serialNumber", true)->value();
  }
  
  if (request->hasParam("appId", true)) {
    config_->evaAppId = request->getParam("appId", true)->value();
  }
  
  if (request->hasParam("appSecret", true)) {
    String secret = request->getParam("appSecret", true)->value();
    if (!secret.isEmpty()) {
      config_->evaAppSecret = secret;
    }
  }

  if (request->hasParam("zinvoltEmail", true)) {
    config_->zinvoltEmail = request->getParam("zinvoltEmail", true)->value();
  }

  if (request->hasParam("zinvoltBatteryId", true)) {
    config_->zinvoltBatteryId = request->getParam("zinvoltBatteryId", true)->value();
  }

  if (request->hasParam("zinvoltPassword", true)) {
    String password = request->getParam("zinvoltPassword", true)->value();
    if (!password.isEmpty()) {
      config_->zinvoltPassword = password;
    }
  }
  
  // Save to NVS
  config_->save(preferences);
  
  Serial.println("Battery API configuration updated");
  Serial.printf("  Enabled: %s\n", config_->evaEnabled ? "Yes" : "No");
  Serial.printf("  Backend: %s\n", config_->batteryBackend.c_str());
  if (config_->batteryBackend == "alphaess") {
    Serial.printf("  Serial Number: %s\n", config_->evaSerialNumber.c_str());
  } else {
    Serial.printf("  Email: %s\n", config_->zinvoltEmail.c_str());
    Serial.printf("  Battery ID: %s\n", config_->zinvoltBatteryId.c_str());
  }
  
  request->send(200, "application/json", 
                "{\"success\":true,\"message\":\"Battery API settings saved.\"}");
}

void WebInterface::handleGetOptimizeConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["deliverySetpoint"] = config_->optimizeDeliverySetpoint;
  doc["minEvaActivity"] = config_->optimizeMinEvaActivity;
  doc["minSolarPower"] = config_->optimizeMinSolarPower;
  doc["solarThreshold"] = config_->optimizeSolarThreshold;
  doc["highSolarSetpoint"] = config_->optimizeHighSolarSetpoint;
  doc["filterTimeConstant"] = config_->optimizeFilterTimeConstant;
  doc["minDeliveryForAdjust"] = config_->optimizeMinDeliveryForAdjust;
  doc["adjustDivisor"] = config_->optimizeAdjustDivisor;
  doc["toleranceLow"] = config_->optimizeToleranceLow;
  doc["toleranceHigh"] = config_->optimizeToleranceHigh;
  doc["largeErrorThreshold"] = config_->optimizeLargeErrorThreshold;
  doc["integratorReduction"] = config_->optimizeIntegratorReduction;
  doc["hysteresisDelivery"] = config_->optimizeHysteresisDelivery;
  doc["hysteresisAdjustment"] = config_->optimizeHysteresisAdjustment;
  doc["integratorMin"] = config_->optimizeIntegratorMin;
  doc["integratorMax"] = config_->optimizeIntegratorMax;
  doc["integratorStep"] = config_->optimizeIntegratorStep;
  doc["errorDeadband"] = config_->optimizeErrorDeadband;
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetOptimizeConfig(AsyncWebServerRequest* request) {
  bool updated = false;
  
  if (request->hasParam("deliverySetpoint", true)) {
    config_->optimizeDeliverySetpoint = request->getParam("deliverySetpoint", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("minEvaActivity", true)) {
    config_->optimizeMinEvaActivity = request->getParam("minEvaActivity", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("minSolarPower", true)) {
    config_->optimizeMinSolarPower = request->getParam("minSolarPower", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("solarThreshold", true)) {
    config_->optimizeSolarThreshold = request->getParam("solarThreshold", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("highSolarSetpoint", true)) {
    config_->optimizeHighSolarSetpoint = request->getParam("highSolarSetpoint", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("filterTimeConstant", true)) {
    config_->optimizeFilterTimeConstant = request->getParam("filterTimeConstant", true)->value().toFloat();
    if (config_->optimizeFilterTimeConstant < 1.0f) {
      config_->optimizeFilterTimeConstant = 1.0f;
    }
    updated = true;
  }
  if (request->hasParam("minDeliveryForAdjust", true)) {
    config_->optimizeMinDeliveryForAdjust = request->getParam("minDeliveryForAdjust", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("adjustDivisor", true)) {
    config_->optimizeAdjustDivisor = request->getParam("adjustDivisor", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("toleranceLow", true)) {
    config_->optimizeToleranceLow = request->getParam("toleranceLow", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("toleranceHigh", true)) {
    config_->optimizeToleranceHigh = request->getParam("toleranceHigh", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("largeErrorThreshold", true)) {
    config_->optimizeLargeErrorThreshold = request->getParam("largeErrorThreshold", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("integratorReduction", true)) {
    config_->optimizeIntegratorReduction = request->getParam("integratorReduction", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("hysteresisDelivery", true)) {
    config_->optimizeHysteresisDelivery = request->getParam("hysteresisDelivery", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("hysteresisAdjustment", true)) {
    config_->optimizeHysteresisAdjustment = request->getParam("hysteresisAdjustment", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("integratorMin", true)) {
    config_->optimizeIntegratorMin = request->getParam("integratorMin", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("integratorMax", true)) {
    config_->optimizeIntegratorMax = request->getParam("integratorMax", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("integratorStep", true)) {
    config_->optimizeIntegratorStep = request->getParam("integratorStep", true)->value().toFloat();
    updated = true;
  }
  if (request->hasParam("errorDeadband", true)) {
    config_->optimizeErrorDeadband = request->getParam("errorDeadband", true)->value().toFloat();
    updated = true;
  }
  
  if (updated) {
    config_->save(preferences);
    Serial.println("Optimize mode settings saved");
    request->send(200, "application/json", 
                  "{\"success\":true,\"message\":\"Optimize settings saved.\"}");
  } else {
    request->send(400, "application/json", 
                  "{\"error\":\"No valid parameters provided\"}");
  }
}

void WebInterface::handleNotFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

String WebInterface::getStatusJSON() {
  JsonDocument doc;
  
  // System info
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["chipModel"] = ESP.getChipModel();
  doc["firmwareVersion"] = getFirmwareVersion();
  
  // WiFi info
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  
  // MQTT status - get from main.cpp
  extern bool getMqttConnected();
  doc["mqtt"]["connected"] = getMqttConnected();
  
  // P1 parser status
  doc["p1"]["valid"] = parser_->isValid();
  doc["p1"]["connected"] = parser_->isValid(); // Alias for connection status
  doc["p1"]["totalPower"] = parser_->getTotalActivePower();
  doc["p1"]["dsmrVersion"] = parser_->getDsmrVersion();
  doc["p1"]["interval"] = modifier_->getTelegramInterval();
  
  // Modifier status
  doc["modifier"]["mode"] = modifier_->getMode();
  doc["modifier"]["modeString"] = modifier_->getModeString();
  doc["modifier"]["batteryPhase"] = modifier_->getBatteryPhase();
  doc["modifier"]["modifyPhase"] = modifier_->getModifyPhase();
  doc["modifier"]["singlePhaseMode"] = modifier_->getSinglePhaseMeterMode();
  doc["modifier"]["forcePower"] = modifier_->getForcePower();
  doc["modifier"]["powerSetpoint"] = modifier_->getPowerSetpoint();
  doc["modifier"]["optimizeSetpoint"] = modifier_->getOptimizeSetpoint();
  doc["modifier"]["optimizeTrace"]["actualW"] = modifier_->getOptimizeTraceActualW();
  doc["modifier"]["optimizeTrace"]["scaledActualW"] = modifier_->getOptimizeTraceScaledActualW();
  doc["modifier"]["optimizeTrace"]["errorW"] = modifier_->getOptimizeTraceErrorW();
  doc["modifier"]["optimizeTrace"]["commandW"] = modifier_->getOptimizeTraceCommandW();
  doc["modifier"]["optimizeTrace"]["targetW"] = modifier_->getOptimizeTraceTargetW();
  doc["modifier"]["optimizeTrace"]["filteredW"] = modifier_->getOptimizeTraceFilteredW();
  doc["modifier"]["optimizeTrace"]["integratorW"] = modifier_->getOptimizeTraceIntegratorW();
  doc["modifier"]["optimizeTrace"]["adjustDivisor"] = modifier_->getOptimizeTraceAdjustDivisor();
  doc["modifier"]["optimizeTrace"]["neutralHold"] = modifier_->getOptimizeTraceNeutralHold();
  doc["optimizeConfig"]["toleranceLow"] = config_->optimizeToleranceLow;
  doc["optimizeConfig"]["toleranceHigh"] = config_->optimizeToleranceHigh;
  
  // Battery data from AlphaESS
  doc["battery"]["backend"] = config_->batteryBackend;
  doc["battery"]["enabled"] = config_->evaEnabled;
  doc["battery"]["soc"] = config_->batterySOC;
  doc["battery"]["power"] = config_->batteryPower;
  doc["battery"]["gridPower"] = config_->gridPower;
  doc["battery"]["solarPower"] = config_->actualSolarPower;
  doc["battery"]["apiSolarPower"] = config_->batteryApiSolarPower;
  doc["battery"]["apiCocPower"] = config_->batteryApiCocPower;
  doc["battery"]["apiMeterPower"] = config_->batteryApiMeterPower;
  
  // Modified power values (for display)
  doc["modifier"]["modifiedPowerL1"] = config_->modifiedPowerL1;
  doc["modifier"]["modifiedPowerL2"] = config_->modifiedPowerL2;
  doc["modifier"]["modifiedPowerL3"] = config_->modifiedPowerL3;
  doc["modifier"]["totalModifiedPower"] = config_->totalModifiedPower;
  
  String response;
  serializeJson(doc, response);
  return response;
}

String WebInterface::getActualsPage() {
  return ::getActualsPage();
}

String WebInterface::getSettingsPage() {
  return ::getSettingsPage();
}

String WebInterface::getUpdatePage() {
  return ::getUpdatePage();
}

void WebInterface::handleSetExternalControl(AsyncWebServerRequest* request) {
  if (!request->hasParam("value")) {
    request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
    return;
  }
  
  float power = request->getParam("value")->value().toFloat();
  
  // Validate power value is within reasonable range (-20000W to 20000W)
  if (power < -20000.0f || power > 20000.0f) {
    request->send(400, "application/json", "{\"error\":\"Power value out of range (-20000 to 20000)\"}");
    return;
  }
  
  // Set external control power and update timestamp
  modifier_->setExternalControlPower(power);
  
  Serial.printf("[WEB] External control power set to: %.1f W\n", power);
  
  // Return confirmation with current mode info
  JsonDocument doc;
  doc["success"] = true;
  doc["externalPower"] = power;
  doc["mode"] = modifier_->getMode();
  doc["modeString"] = modifier_->getModeString();
  doc["timeoutSeconds"] = 60;
  doc["info"] = "External control value received. Will auto-relay after 60 seconds if no new value received.";
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

