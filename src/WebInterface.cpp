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
  _server = server;
  _parser = parser;
  _modifier = modifier;
  _config = config;
}

void WebInterface::begin() {
    auto checkAuth = [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
            request->requestAuthentication();
            return false;
        }
        return true;
    };

    _server->on("/", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleActualsPage(request);
    });

    _server->on("/actuals", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleActualsPage(request);
    });

    _server->on("/settings", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleSettingsPage(request);
    });

    _server->on("/update", HTTP_GET, [this, checkAuth](AsyncWebServerRequest* request) {
        if (!checkAuth(request)) {
            return;
        }
        handleUpdatePage(request);
    });
  
  // REST API endpoints (no authentication required)
  _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetStatus(request);
  });
  
  _server->on("/api/p1data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetP1Data(request);
  });
  
  _server->on("/api/mode", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetMode(request);
  });
  
  _server->on("/api/phase", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetPhase(request);
  });
  
  _server->on("/api/power", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetPower(request);
  });
  
  _server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetConfig(request);
  });
  
  _server->on("/api/mqtt", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetMqttConfig(request);
  });
  
  _server->on("/api/mqtt", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetMqttConfig(request);
  });
  
  _server->on("/api/advanced", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetAdvancedConfig(request);
  });
  
  _server->on("/api/advanced", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetAdvancedConfig(request);
  });
  
  _server->on("/api/eva", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetEvaConfig(request);
  });
  
  _server->on("/api/eva", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSetEvaConfig(request);
  });
  
  _server->on("/api/external", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleSetExternalControl(request);
  });
  
  // 404 handler
  _server->onNotFound([this](AsyncWebServerRequest* request) {
    handleNotFound(request);
  });

  // Manual OTA update handler with authentication and validation
  _server->on("/update", HTTP_POST, 
    [this](AsyncWebServerRequest *request) {
      if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
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
      if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
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
  
  doc["valid"] = _parser->isValid();
  doc["timestamp"] = _parser->getTimestamp();
  
  // Power data
  JsonObject power = doc["power"].to<JsonObject>();
    power["total"] = _config->actualTotalPower;
    power["l1"] = _config->actualPowerL1;
    power["l2"] = _config->actualPowerL2;
    power["l3"] = _config->actualPowerL3;

    // Modified power data (kept in sync with last processed telegram)
    JsonObject mod = doc["modifier"].to<JsonObject>();
    mod["total"] = _config->totalModifiedPower;
    mod["l1"] = _config->modifiedPowerL1;
    mod["l2"] = _config->modifiedPowerL2;
    mod["l3"] = _config->modifiedPowerL3;
  
  // Voltage data
  JsonObject voltage = doc["voltage"].to<JsonObject>();
  voltage["l1"] = _parser->getVoltageL1();
  voltage["l2"] = _parser->getVoltageL2();
  voltage["l3"] = _parser->getVoltageL3();
  
  // Current data
  JsonObject current = doc["current"].to<JsonObject>();
  current["l1"] = _parser->getCurrentL1();
  current["l2"] = _parser->getCurrentL2();
  current["l3"] = _parser->getCurrentL3();
  
  // Energy data
  JsonObject energy = doc["energy"].to<JsonObject>();
    float totalImport = _parser->getTotalEnergyImport();
    float totalExport = _parser->getTotalEnergyExport();
    energy["import"] = totalImport;   // lifetime (kWh)
    energy["export"] = totalExport;   // lifetime (kWh)
    // Today's values: (current - baseline at day start)
    float todayImport = totalImport - _config->dayStartEnergyImport;
    float todayExport = totalExport - _config->dayStartEnergyExport;
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
  
  if (mode < 0 || mode > 6) {
    request->send(400, "application/json", "{\"error\":\"Invalid mode value (0-6)\"}");
    return;
  }
  
  _modifier->setMode((OperationMode)mode);
  _config->operationMode = (OperationMode)mode;
  _config->save(preferences);
  
  Serial.printf("Mode changed to: %s\n", _modifier->getModeString().c_str());
  
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
    _modifier->setBatteryPhase(phase);
    _config->batteryPhase = phase;
    _config->save(preferences);
    doc["batteryPhase"] = phase;
    Serial.printf("Battery phase set to: %d\n", phase);
  }
  
  if (request->hasParam("modify")) {
    int phase = request->getParam("modify")->value().toInt();
    if (phase < 1 || phase > 3) {
      request->send(400, "application/json", "{\"error\":\"Invalid modify phase (1-3)\"}");
      return;
    }
    _modifier->setModifyPhase(phase);
    _config->modifyPhase = phase;
    _config->save(preferences);
    doc["modifyPhase"] = phase;
    Serial.printf("Modify phase set to: %d\n", phase);
  }
  
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
  
  _modifier->setForcePower(power);
  _config->forcePower = power;
  _config->save(preferences);
  
  Serial.printf("Force power set to: %.1f W\n", power);
  
  request->send(200, "application/json", "{\"success\":true,\"power\":" + String(power) + "}");
}

void WebInterface::handleGetConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["mode"] = _modifier->getMode();
  doc["modeString"] = _modifier->getModeString();
  doc["batteryPhase"] = _modifier->getBatteryPhase();
  doc["modifyPhase"] = _modifier->getModifyPhase();
  doc["forcePower"] = _modifier->getForcePower();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleGetMqttConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["server"] = _config->mqttServer;
  doc["port"] = _config->mqttPort;
  doc["user"] = _config->mqttUser;
  // Don't send password for security
  doc["hasPassword"] = !_config->mqttPassword.isEmpty();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetMqttConfig(AsyncWebServerRequest* request) {
  if (!request->hasParam("server", true)) {
    request->send(400, "application/json", "{\"error\":\"Missing server parameter\"}");
    return;
  }
  
  _config->mqttServer = request->getParam("server", true)->value();
  _config->mqttPort = request->hasParam("port", true) ? 
                      request->getParam("port", true)->value().toInt() : 1883;
  
  if (request->hasParam("user", true)) {
    _config->mqttUser = request->getParam("user", true)->value();
  }
  
  if (request->hasParam("password", true)) {
    String pwd = request->getParam("password", true)->value();
    if (!pwd.isEmpty()) {
      _config->mqttPassword = pwd;
    }
  }
  
  // Save to NVS
  _config->save(preferences);
  
  // Trigger MQTT reconnection via callback in main.cpp
  extern void reconnectMqtt();
  reconnectMqtt();

  Serial.println("MQTT configuration updated:");
  Serial.printf("  Server: %s:%d\n", _config->mqttServer.c_str(), _config->mqttPort);
  Serial.printf("  User: %s\n", _config->mqttUser.c_str());
  
  request->send(200, "application/json", 
                "{\"success\":true,\"message\":\"MQTT config saved. Device will reconnect.\"}");
}

void WebInterface::handleGetAdvancedConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["useTxReq"] = _config->useTxReq;
  doc["webUsername"] = _config->webUsername;
  // Don't send password for security
  doc["hasPassword"] = !_config->webPassword.isEmpty();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetAdvancedConfig(AsyncWebServerRequest* request) {
  if (request->hasParam("useTxReq", true)) {
    String value = request->getParam("useTxReq", true)->value();
    _config->useTxReq = (value == "true" || value == "1");
  }
  
  if (request->hasParam("webUsername", true)) {
    _config->webUsername = request->getParam("webUsername", true)->value();
  }
  
  if (request->hasParam("webPassword", true)) {
    String pwd = request->getParam("webPassword", true)->value();
    if (!pwd.isEmpty()) {
      _config->webPassword = pwd;
    }
  }
  
  // Save to NVS
  _config->save(preferences);
  
  Serial.println("Advanced configuration updated");
  Serial.printf("  Use TXREQ: %s\n", _config->useTxReq ? "Yes" : "No");
  Serial.printf("  Web Username: %s\n", _config->webUsername.c_str());
  
  request->send(200, "application/json", 
                "{\"success\":true,\"message\":\"Advanced settings saved.\"}");
}

void WebInterface::handleGetEvaConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["enabled"] = _config->evaEnabled;
  doc["serialNumber"] = _config->evaSerialNumber;
  doc["appId"] = _config->evaAppId;
  doc["hasSecret"] = !_config->evaAppSecret.isEmpty();
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleSetEvaConfig(AsyncWebServerRequest* request) {
  if (request->hasParam("enabled", true)) {
    String value = request->getParam("enabled", true)->value();
    _config->evaEnabled = (value == "true" || value == "1");
  }
  
  if (request->hasParam("serialNumber", true)) {
    _config->evaSerialNumber = request->getParam("serialNumber", true)->value();
  }
  
  if (request->hasParam("appId", true)) {
    _config->evaAppId = request->getParam("appId", true)->value();
  }
  
  if (request->hasParam("appSecret", true)) {
    String secret = request->getParam("appSecret", true)->value();
    if (!secret.isEmpty()) {
      _config->evaAppSecret = secret;
    }
  }
  
  // Save to NVS
  _config->save(preferences);
  
  Serial.println("AlphaESS configuration updated");
  Serial.printf("  Enabled: %s\n", _config->evaEnabled ? "Yes" : "No");
  Serial.printf("  Serial Number: %s\n", _config->evaSerialNumber.c_str());
  
  request->send(200, "application/json", 
                "{\"success\":true,\"message\":\"AlphaESS settings saved.\"}");
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
  doc["p1"]["valid"] = _parser->isValid();
  doc["p1"]["totalPower"] = _parser->getTotalActivePower();
  
  // Modifier status
  doc["modifier"]["mode"] = _modifier->getMode();
  doc["modifier"]["modeString"] = _modifier->getModeString();
  doc["modifier"]["batteryPhase"] = _modifier->getBatteryPhase();
  doc["modifier"]["modifyPhase"] = _modifier->getModifyPhase();
  doc["modifier"]["forcePower"] = _modifier->getForcePower();
  
  // Battery data from AlphaESS
  doc["battery"]["soc"] = _config->batterySOC;
  doc["battery"]["power"] = _config->batteryPower;
  doc["battery"]["gridPower"] = _config->gridPower;
  
  // Modified power values (for display)
  doc["modifier"]["modifiedPowerL1"] = _config->modifiedPowerL1;
  doc["modifier"]["modifiedPowerL2"] = _config->modifiedPowerL2;
  doc["modifier"]["modifiedPowerL3"] = _config->modifiedPowerL3;
  doc["modifier"]["totalModifiedPower"] = _config->totalModifiedPower;
  
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
  _modifier->setExternalControlPower(power);
  
  Serial.printf("[WEB] External control power set to: %.1f W\n", power);
  
  // Return confirmation with current mode info
  JsonDocument doc;
  doc["success"] = true;
  doc["externalPower"] = power;
  doc["mode"] = _modifier->getMode();
  doc["modeString"] = _modifier->getModeString();
  doc["timeoutSeconds"] = 60;
  doc["info"] = "External control value received. Will auto-relay after 60 seconds if no new value received.";
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

