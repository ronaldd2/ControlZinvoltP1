/*
 * WebInterface.cpp - Implementation of web server and REST API
 */

#include "WebInterface.h"
#include <Preferences.h>

// External references declared in main.cpp
extern Preferences preferences;

// Firmware version (build date/time)
#define FIRMWARE_VERSION "2026-01-20 12:00"

static const char PAGE_STYLES[] = R"rawliteral(
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        .header p {
            font-size: 1.1em;
            opacity: 0.9;
        }
        .page-links {
            margin-top: 16px;
            display: flex;
            justify-content: center;
            gap: 12px;
        }
        .page-link {
            padding: 10px 18px;
            border-radius: 999px;
            border: 1px solid rgba(255, 255, 255, 0.5);
            color: white;
            font-weight: 600;
            text-decoration: none;
            transition: background 0.3s, color 0.3s;
        }
        .page-link.active {
            background: white;
            color: #3e2c7f;
            border-color: white;
            cursor: default;
        }
        .page-link:hover {
            background: rgba(255, 255, 255, 0.2);
        }
        .card {
            background: white;
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 8px 16px rgba(0, 0, 0, 0.1);
        }
        .card h2 {
            color: #667eea;
            margin-bottom: 20px;
            font-size: 1.5em;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
        }
        .metric {
            padding: 15px;
            background: #f8f9fa;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }
        .metric-label {
            font-size: 0.9em;
            color: #666;
            margin-bottom: 5px;
        }
        .metric-value {
            font-size: 1.8em;
            font-weight: bold;
            color: #333;
        }
        .metric-unit {
            font-size: 0.8em;
            color: #888;
            margin-left: 5px;
        }
        .control-group {
            margin-bottom: 20px;
        }
        .control-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: 500;
            color: #333;
        }
        select,
        input {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 1em;
            transition: border-color 0.3s;
        }
        select:focus,
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            background: #667eea;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 6px;
            font-size: 1em;
            cursor: pointer;
            transition: background 0.3s;
            margin-top: 10px;
        }
        button:hover {
            background: #5568d3;
        }
        .status-badge {
            display: inline-block;
            padding: 6px 12px;
            border-radius: 20px;
            font-size: 0.9em;
            font-weight: 500;
        }
        .status-active {
            background: #d4edda;
            color: #155724;
        }
        .status-inactive {
            background: #f8d7da;
            color: #721c24;
        }
        .phase-selector {
            display: flex;
            gap: 10px;
            margin-top: 10px;
        }
        .phase-btn {
            flex: 1;
            padding: 10px;
            border: 2px solid #ddd;
            background: white;
            border-radius: 6px;
            cursor: pointer;
            transition: all 0.3s;
        }
        .phase-btn.active {
            background: #667eea;
            color: white;
            border-color: #667eea;
        }
        @media (max-width: 768px) {
            .header h1 {
                font-size: 1.8em;
            }
            .grid {
                grid-template-columns: 1fr;
            }
            .page-links {
                flex-direction: column;
            }
        }
    </style>
)rawliteral";

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
  
  // REST API endpoints (with authentication)
  _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleGetStatus(request);
  });
  
  _server->on("/api/p1data", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleGetP1Data(request);
  });
  
  _server->on("/api/mode", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleSetMode(request);
  });
  
  _server->on("/api/phase", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleSetPhase(request);
  });
  
  _server->on("/api/power", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleSetPower(request);
  });
  
  _server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleGetConfig(request);
  });
  
  _server->on("/api/mqtt", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleGetMqttConfig(request);
  });
  
  _server->on("/api/mqtt", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleSetMqttConfig(request);
  });
  
  _server->on("/api/advanced", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleGetAdvancedConfig(request);
  });
  
  _server->on("/api/advanced", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleSetAdvancedConfig(request);
  });
  
  _server->on("/api/eva", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleGetEvaConfig(request);
  });
  
  _server->on("/api/eva", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!request->authenticate(_config->webUsername.c_str(), _config->webPassword.c_str())) {
      return request->requestAuthentication();
    }
    handleSetEvaConfig(request);
  });
  
  // 404 handler
  _server->onNotFound([this](AsyncWebServerRequest* request) {
    handleNotFound(request);
  });
  
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
  
  if (mode < 0 || mode > 5) {
    request->send(400, "application/json", "{\"error\":\"Invalid mode value\"}");
    return;
  }
  
  _modifier->setMode((OperationMode)mode);
  
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
  doc["firmwareVersion"] = FIRMWARE_VERSION;
  
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
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ControlZinvoltP1 - Actuals</title>
)rawliteral";
  html += PAGE_STYLES;
  html += R"rawliteral(
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>⚡ ControlZinvoltP1</h1>
            <p>ESP32-S3 P1 Controller for Zinvolt VT1000</p>
            <div class="page-links">
                <span class="page-link active">Actuals</span>
                <a class="page-link" href="/settings">Settings</a>
            </div>
        </div>

        <div class="card">
            <h2>📊 Real-time Smart Meter Data</h2>
            <div class="grid">
                <div class="metric">
                    <div class="metric-label">Total Power</div>
                    <div class="metric-value" id="totalPower">0<span class="metric-unit">W</span></div>
                    <div style="font-size: 0.85em; color: #856404; margin-top: 4px;">
                        Modified: <strong id="totalModifiedPower">0</strong> W
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L1 Power</div>
                    <div class="metric-value" id="powerL1">0<span class="metric-unit">W</span></div>
                    <div style="font-size: 0.85em; color: #856404; margin-top: 4px;">
                        Modified: <strong id="modifiedPowerL1">0</strong> W
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L2 Power</div>
                    <div class="metric-value" id="powerL2">0<span class="metric-unit">W</span></div>
                    <div style="font-size: 0.85em; color: #856404; margin-top: 4px;">
                        Modified: <strong id="modifiedPowerL2">0</strong> W
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L3 Power</div>
                    <div class="metric-value" id="powerL3">0<span class="metric-unit">W</span></div>
                    <div style="font-size: 0.85em; color: #856404; margin-top: 4px;">
                        Modified: <strong id="modifiedPowerL3">0</strong> W
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">Energy Consumed Today</div>
                    <div class="metric-value" id="energyImport">0<span class="metric-unit">kWh</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Energy Produced Today</div>
                    <div class="metric-value" id="energyExport">0<span class="metric-unit">kWh</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Current L1 / L2 / L3</div>
                    <div class="metric-value" style="font-size: 1.0em;" id="currentAll">0 / 0 / 0<span class="metric-unit">A</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Current Mode</div>
                    <div class="metric-value" style="font-size: 1.2em;" id="currentMode">Loading...</div>
                </div>
            </div>
            
            <div style="margin-top: 20px; padding: 12px; background: #f0f0f0; border-radius: 6px; font-size: 0.85em;">
                <details>
                    <summary style="cursor: pointer; font-weight: 500; color: #666;">⚡ Voltage Details (if available)</summary>
                    <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; margin-top: 10px;">
                        <div style="padding: 8px; background: white; border-radius: 4px;">
                            <div style="font-size: 0.9em; color: #888;">L1</div>
                            <div style="font-weight: bold;" id="voltageL1">-<span style="font-weight: normal; font-size: 0.9em;"> V</span></div>
                        </div>
                        <div style="padding: 8px; background: white; border-radius: 4px;">
                            <div style="font-size: 0.9em; color: #888;">L2</div>
                            <div style="font-weight: bold;" id="voltageL2">-<span style="font-weight: normal; font-size: 0.9em;"> V</span></div>
                        </div>
                        <div style="padding: 8px; background: white; border-radius: 4px;">
                            <div style="font-size: 0.9em; color: #888;">L3</div>
                            <div style="font-weight: bold;" id="voltageL3">-<span style="font-weight: normal; font-size: 0.9em;"> V</span></div>
                        </div>
                    </div>
                </details>
            </div>
        </div>

        <div class="card">
            <h2>🔋 Battery Status (AlphaESS)</h2>
            <div class="grid">
                <div class="metric">
                    <div class="metric-label">Battery SOC</div>
                    <div class="metric-value" id="batterySOC">-<span class="metric-unit">%</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Battery Power</div>
                    <div class="metric-value" id="batteryPower">-<span class="metric-unit">W</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Grid Power</div>
                    <div class="metric-value" id="gridPower">-<span class="metric-unit">W</span></div>
                </div>
            </div>
            <div id="evaDis abled" style="display: none; margin-top: 10px; padding: 10px; background: #f8f9fa; border-radius: 6px; color: #666; font-size: 0.9em;">
                AlphaESS integration is disabled. Enable it in <a href="/settings" style="color: #667eea;">Settings</a>.
            </div>
        </div>

        <div class="card">
            <h2>ℹ️ System Information</h2>
            <div class="grid">
                <div class="metric">
                    <div class="metric-label">Uptime</div>
                    <div class="metric-value" id="uptime">0<span class="metric-unit">s</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">WiFi SSID</div>
                    <div class="metric-value" style="font-size: 1.2em;" id="wifiSSID">-</div>
                </div>
                <div class="metric">
                    <div class="metric-label">IP Address</div>
                    <div class="metric-value" style="font-size: 1.1em;" id="ipAddress">-</div>
                </div>
                <div class="metric">
                    <div class="metric-label">WiFi Signal</div>
                    <div class="metric-value" id="wifiRSSI">0<span class="metric-unit">dBm</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">MQTT Status</div>
                    <div class="metric-value">
                        <span id="mqttConnectionStatus" class="status-badge status-inactive">Loading...</span>
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">P1 Link</div>
                    <div class="metric-value" id="p1Validity">Checking...</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Free Heap</div>
                    <div class="metric-value" id="freeHeap">0<span class="metric-unit">B</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Chip Model</div>
                    <div class="metric-value" id="chipModel">-</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Firmware Version</div>
                    <div class="metric-value" style="font-size: 1.0em;" id="firmwareVersion">-</div>
                </div>
            </div>
        </div>
    </div>

    <script>
        async function fetchData() {
            try {
                const p1Response = await fetch('/api/p1data');
                const p1Data = await p1Response.json();

                if (p1Data.valid) {
                    const power = p1Data.power || {};
                    const modPower = p1Data.modifier || {};
                    const energy = p1Data.energy || {};
                    const current = p1Data.current || { l1: 0, l2: 0, l3: 0 };
                    const voltage = p1Data.voltage || {};

                    document.getElementById('totalPower').innerHTML = Math.round((power.total || 0) * 1000) + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL1').innerHTML = Math.round((power.l1 || 0) * 1000) + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL2').innerHTML = Math.round((power.l2 || 0) * 1000) + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL3').innerHTML = Math.round((power.l3 || 0) * 1000) + '<span class="metric-unit">W</span>';

                    // Synchronous modified values (same telegram as actuals)
                    document.getElementById('totalModifiedPower').textContent = Math.round((modPower.total || 0) * 1000);
                    document.getElementById('modifiedPowerL1').textContent = Math.round((modPower.l1 || 0) * 1000);
                    document.getElementById('modifiedPowerL2').textContent = Math.round((modPower.l2 || 0) * 1000);
                    document.getElementById('modifiedPowerL3').textContent = Math.round((modPower.l3 || 0) * 1000);

                    const todayImport = (energy.todayImport != null) ? energy.todayImport : (energy.import || 0);
                    const todayExport = (energy.todayExport != null) ? energy.todayExport : (energy.export || 0);
                    document.getElementById('energyImport').innerHTML = todayImport.toFixed(2) + '<span class="metric-unit">kWh</span>';
                    document.getElementById('energyExport').innerHTML = todayExport.toFixed(2) + '<span class="metric-unit">kWh</span>';

                    document.getElementById('currentAll').innerHTML =
                        current.l1.toFixed(1) + ' / ' +
                        current.l2.toFixed(1) + ' / ' +
                        current.l3.toFixed(1) + '<span class="metric-unit">A</span>';

                    document.getElementById('voltageL1').innerHTML = (voltage.l1 > 0)
                        ? voltage.l1.toFixed(1) + '<span style="font-weight: normal; font-size: 0.9em;"> V</span>'
                        : '-';
                    document.getElementById('voltageL2').innerHTML = (voltage.l2 > 0)
                        ? voltage.l2.toFixed(1) + '<span style="font-weight: normal; font-size: 0.9em;"> V</span>'
                        : '-';
                    document.getElementById('voltageL3').innerHTML = (voltage.l3 > 0)
                        ? voltage.l3.toFixed(1) + '<span style="font-weight: normal; font-size: 0.9em;"> V</span>'
                        : '-';
                }

                const statusResponse = await fetch('/api/status');
                const statusData = await statusResponse.json();

                const modifier = statusData.modifier || {};
                const wifi = statusData.wifi || {};
                document.getElementById('currentMode').textContent = modifier.modeString || '—';
                
                document.getElementById('uptime').innerHTML = (statusData.uptime || 0) + '<span class="metric-unit">s</span>';
                document.getElementById('wifiSSID').textContent = wifi.ssid || '-';
                document.getElementById('ipAddress').textContent = wifi.ip || '-';
                document.getElementById('wifiRSSI').innerHTML = (wifi.rssi || 0) + '<span class="metric-unit">dBm</span>';
                document.getElementById('freeHeap').innerHTML = (statusData.freeHeap || 0) + '<span class="metric-unit">B</span>';
                document.getElementById('chipModel').textContent = statusData.chipModel || '-';
                document.getElementById('firmwareVersion').textContent = statusData.firmwareVersion || '-';

                const mqttStatusEl = document.getElementById('mqttConnectionStatus');
                if (statusData.mqtt && statusData.mqtt.connected) {
                    mqttStatusEl.textContent = '✓ Connected';
                    mqttStatusEl.className = 'status-badge status-active';
                } else {
                    mqttStatusEl.textContent = '✗ Disconnected';
                    mqttStatusEl.className = 'status-badge status-inactive';
                }

                document.getElementById('p1Validity').textContent = (statusData.p1 && statusData.p1.valid) ? 'Valid' : 'Invalid';

                // Update battery data
                if (statusData.battery) {
                    document.getElementById('batterySOC').innerHTML = (statusData.battery.soc || 0) + '<span class="metric-unit">%</span>';
                    const batteryPower = statusData.battery.power || 0;
                    const powerSign = batteryPower > 10 ? '⚡ Discharging' : batteryPower < -10 ? '🔋 Charging' : '🔌 Standby' ;
                    document.getElementById('batteryPower').innerHTML = Math.abs(batteryPower) + '<span class="metric-unit">W</span> ' + powerSign;
                    document.getElementById('gridPower').innerHTML = (statusData.battery.gridPower || 0) + '<span class="metric-unit">W</span>';
                } else {
                    document.getElementById('batterySOC').innerHTML = '-<span class="metric-unit">%</span>';
                    document.getElementById('batteryPower').innerHTML = '-<span class="metric-unit">W</span>';
                    document.getElementById('gridPower').innerHTML = '-<span class="metric-unit">W</span>';
                }
            } catch (error) {
                console.error('Error fetching data:', error);
            }
        }

        fetchData();
        setInterval(fetchData, 2000);
    </script>
</body>
</html>
)rawliteral";

  return html;
}

String WebInterface::getSettingsPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ControlZinvoltP1 - Settings</title>
)rawliteral";
  html += PAGE_STYLES;
  html += R"rawliteral(
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>⚡ ControlZinvoltP1</h1>
            <p>ESP32-S3 P1 Controller for Zinvolt VT1000</p>
            <div class="page-links">
                <a class="page-link" href="/">Actuals</a>
                <span class="page-link active">Settings</span>
            </div>
        </div>

        <div class="card">
            <h2>🎛️ Control Settings</h2>
            <p style="color: #666; font-size: 0.9em; margin-bottom: 20px;">These settings control P1 telegram modification and battery behavior. Changes are also published to MQTT/Home Assistant.</p>

            <div class="control-group">
                <label>Operation Mode <span style="color: #667eea; font-size: 0.85em;">(MQTT: operation_mode)</span></label>
                <select id="modeSelect">
                    <option value="0">Unmodified Forward</option>
                    <option value="1">Off</option>
                    <option value="2">Force Charge</option>
                    <option value="3">Force Discharge</option>
                    <option value="4">Charge Only</option>
                    <option value="5">Discharge Only</option>
                </select>
                <button onclick="setMode()">Set Mode</button>
            </div>

            <div class="control-group">
                <label>Force Power (Watts) <span style="color: #667eea; font-size: 0.85em;">(MQTT: force_power)</span></label>
                <input type="number" id="forcePower" min="0" max="20000" step="100" value="3000">
                <button onclick="setPower()">Set Power</button>
            </div>

            <div class="control-group">
                <label>Battery Phase <span style="color: #667eea; font-size: 0.85em;">(MQTT: battery_phase)</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Which phase the Zinvolt battery is physically connected to</small>
                <div class="phase-selector">
                    <button class="phase-btn" id="battPhase1" onclick="setBatteryPhase(1)">L1</button>
                    <button class="phase-btn" id="battPhase2" onclick="setBatteryPhase(2)">L2</button>
                    <button class="phase-btn" id="battPhase3" onclick="setBatteryPhase(3)">L3</button>
                </div>
            </div>

            <div class="control-group">
                <label>Modify Phase <span style="color: #667eea; font-size: 0.85em;">(MQTT: modify_phase)</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Which phase in the P1 telegram to modify for battery control</small>
                <div class="phase-selector">
                    <button class="phase-btn" id="modPhase1" onclick="setModifyPhase(1)">L1</button>
                    <button class="phase-btn" id="modPhase2" onclick="setModifyPhase(2)">L2</button>
                    <button class="phase-btn" id="modPhase3" onclick="setModifyPhase(3)">L3</button>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>📡 MQTT Configuration</h2>

            <div class="control-group">
                <label>MQTT Broker Address</label>
                <input type="text" id="mqttServer" placeholder="homeassistant.local or 192.168.1.100">
            </div>

            <div class="control-group">
                <label>MQTT Port</label>
                <input type="number" id="mqttPort" value="1883" min="1" max="65535">
            </div>

            <div class="control-group">
                <label>MQTT Username (optional)</label>
                <input type="text" id="mqttUser" placeholder="Leave empty for no auth">
            </div>

            <div class="control-group">
                <label>MQTT Password (optional)</label>
                <input type="password" id="mqttPassword" placeholder="Leave empty to keep current">
            </div>

            <button onclick="saveMqttConfig()">Save MQTT Configuration</button>

            <div id="mqttConfigStatus" style="margin-top: 15px; padding: 10px; border-radius: 6px; display: none;"></div>
        </div>

        <div class="card">
            <h2>🔋 AlphaESS Battery Integration</h2>
            <p style="color: #666; font-size: 0.9em; margin-bottom: 20px;">Connect to AlphaESS cloud API to fetch real-time battery data (SOC, power). Data is fetched every 10 seconds and published to MQTT.</p>

            <div class="control-group">
                <label style="display: flex; align-items: center; gap: 10px;">
                    <input type="checkbox" id="evaEnabled" style="width: auto;">
                    <span>Enable AlphaESS Integration</span>
                </label>
            </div>

            <div class="control-group">
                <label>Serial Number (sysSn)</label>
                <input type="text" id="evaSerialNumber" placeholder="ALG0011243456789">
            </div>

            <div class="control-group">
                <label>App ID</label>
                <input type="text" id="evaAppId" placeholder="Your AlphaESS App ID">
            </div>

            <div class="control-group">
                <label>App Secret</label>
                <input type="password" id="evaAppSecret" placeholder="Your AlphaESS App Secret">
            </div>

            <button onclick="saveEvaConfig()">Save AlphaESS Configuration</button>

            <div id="evaConfigStatus" style="margin-top: 15px; padding: 10px; border-radius: 6px; display: none;"></div>
        </div>

        <div class="card">
            <h2>⚙️ Advanced Settings</h2>

            <div class="control-group">
                <label style="display: flex; align-items: center; gap: 10px;">
                    <input type="checkbox" id="useTxReq" style="width: auto;">
                    <span>Enable TXREQ Pin Check (EN Pin)</span>
                </label>
                <small style="color: #666; margin-top: 5px; display: block;">
                    Only enable if your P1 output device uses the TXREQ (EN) pin on GPIO41.
                    Most devices don't need this.
                </small>
            </div>

            <div class="control-group">
                <label>Web Username</label>
                <input type="text" id="webUsername" placeholder="admin">
            </div>

            <div class="control-group">
                <label>Web Password</label>
                <input type="password" id="webPassword" placeholder="Leave empty to keep current">
            </div>

            <button onclick="saveAdvancedSettings()">Save Advanced Settings</button>

            <div id="advancedStatus" style="margin-top: 15px; padding: 10px; border-radius: 6px; display: none;"></div>
        </div>
    </div>

    <script>
        let currentBatteryPhase = 1;
        let currentModifyPhase = 1;

        async function loadConfig() {
            try {
                const response = await fetch('/api/config');
                const data = await response.json();
                if (data.mode != null) {
                    document.getElementById('modeSelect').value = data.mode;
                }
                document.getElementById('forcePower').value = data.forcePower || 3000;

                currentBatteryPhase = data.batteryPhase || 1;
                currentModifyPhase = data.modifyPhase || 1;
                updatePhaseButtons();
            } catch (error) {
                console.error('Error loading config:', error);
            }
        }

        async function loadMqttConfig() {
            try {
                const mqttResponse = await fetch('/api/mqtt');
                const mqttData = await mqttResponse.json();
                document.getElementById('mqttServer').value = mqttData.server || '';
                document.getElementById('mqttPort').value = mqttData.port || 1883;
                document.getElementById('mqttUser').value = mqttData.user || '';
            } catch (error) {
                console.error('Error fetching MQTT config:', error);
            }
        }

        async function loadAdvancedConfig() {
            try {
                const response = await fetch('/api/advanced');
                const data = await response.json();
                document.getElementById('useTxReq').checked = data.useTxReq || false;
                document.getElementById('webUsername').value = data.webUsername || 'admin';
            } catch (error) {
                console.error('Error fetching advanced config:', error);
            }
        }

        function updatePhaseButtons() {
            for (let i = 1; i <= 3; i++) {
                const battBtn = document.getElementById('battPhase' + i);
                if (i === currentBatteryPhase) {
                    battBtn.classList.add('active');
                } else {
                    battBtn.classList.remove('active');
                }
                const modBtn = document.getElementById('modPhase' + i);
                if (i === currentModifyPhase) {
                    modBtn.classList.add('active');
                } else {
                    modBtn.classList.remove('active');
                }
            }
        }

        async function setMode() {
            const mode = document.getElementById('modeSelect').value;
            try {
                const response = await fetch('/api/mode?value=' + mode);
                const data = await response.json();
                if (data.success) {
                    alert('Mode updated successfully!');
                    loadConfig();
                }
            } catch (error) {
                alert('Error setting mode: ' + error);
            }
        }

        async function setPower() {
            const power = document.getElementById('forcePower').value;
            try {
                const response = await fetch('/api/power?value=' + power);
                const data = await response.json();
                if (data.success) {
                    alert('Force power updated successfully!');
                    loadConfig();
                }
            } catch (error) {
                alert('Error setting power: ' + error);
            }
        }

        async function setBatteryPhase(phase) {
            try {
                const response = await fetch('/api/phase?battery=' + phase);
                const data = await response.json();
                if (data.success) {
                    currentBatteryPhase = phase;
                    updatePhaseButtons();
                }
            } catch (error) {
                alert('Error setting battery phase: ' + error);
            }
        }

        async function setModifyPhase(phase) {
            try {
                const response = await fetch('/api/phase?modify=' + phase);
                const data = await response.json();
                if (data.success) {
                    currentModifyPhase = phase;
                    updatePhaseButtons();
                }
            } catch (error) {
                alert('Error setting modify phase: ' + error);
            }
        }

        async function saveMqttConfig() {
            const server = document.getElementById('mqttServer').value;
            const port = document.getElementById('mqttPort').value;
            const user = document.getElementById('mqttUser').value;
            const password = document.getElementById('mqttPassword').value;

            if (!server) {
                showMqttConfigStatus('Please enter MQTT broker address', false);
                return;
            }

            const formData = new FormData();
            formData.append('server', server);
            formData.append('port', port);
            if (user) formData.append('user', user);
            if (password) formData.append('password', password);

            try {
                const response = await fetch('/api/mqtt', {
                    method: 'POST',
                    body: formData
                });
                const data = await response.json();

                if (data.success) {
                    showMqttConfigStatus('MQTT configuration saved! Device will reconnect...', true);
                    document.getElementById('mqttPassword').value = '';
                    setTimeout(() => {
                        showMqttConfigStatus('Restarting device to apply changes...', true);
                        setTimeout(() => location.reload(), 3000);
                    }, 2000);
                } else {
                    showMqttConfigStatus('Error: ' + (data.error || 'Unknown error'), false);
                }
            } catch (error) {
                showMqttConfigStatus('Error saving MQTT config: ' + error, false);
            }
        }

        function showMqttConfigStatus(message, success) {
            const statusDiv = document.getElementById('mqttConfigStatus');
            statusDiv.textContent = message;
            statusDiv.style.display = 'block';
            statusDiv.style.background = success ? '#d4edda' : '#f8d7da';
            statusDiv.style.color = success ? '#155724' : '#721c24';
        }

        async function saveAdvancedSettings() {
            const useTxReq = document.getElementById('useTxReq').checked;
            const webUsername = document.getElementById('webUsername').value;
            const webPassword = document.getElementById('webPassword').value;

            if (!webUsername) {
                showAdvancedStatus('Please enter a username', false);
                return;
            }

            const formData = new FormData();
            formData.append('useTxReq', useTxReq ? 'true' : 'false');
            formData.append('webUsername', webUsername);
            if (webPassword) formData.append('webPassword', webPassword);

            try {
                const response = await fetch('/api/advanced', {
                    method: 'POST',
                    body: formData
                });
                const data = await response.json();

                if (data.success) {
                    showAdvancedStatus('Settings saved! If you changed the password, you may need to log in again.', true);
                    document.getElementById('webPassword').value = '';
                } else {
                    showAdvancedStatus('Error: ' + (data.error || 'Unknown error'), false);
                }
            } catch (error) {
                showAdvancedStatus('Error saving settings: ' + error, false);
            }
        }

        function showAdvancedStatus(message, success) {
            const statusDiv = document.getElementById('advancedStatus');
            statusDiv.textContent = message;
            statusDiv.style.display = 'block';
            statusDiv.style.background = success ? '#d4edda' : '#f8d7da';
            statusDiv.style.color = success ? '#155724' : '#721c24';
        }

        async function loadEvaConfig() {
            try {
                const response = await fetch('/api/eva');
                const data = await response.json();
                document.getElementById('evaEnabled').checked = data.enabled || false;
                document.getElementById('evaSerialNumber').value = data.serialNumber || '';
                document.getElementById('evaAppId').value = data.appId || '';
            } catch (error) {
                console.error('Error fetching EVA config:', error);
            }
        }

        async function saveEvaConfig() {
            const enabled = document.getElementById('evaEnabled').checked;
            const serialNumber = document.getElementById('evaSerialNumber').value;
            const appId = document.getElementById('evaAppId').value;
            const appSecret = document.getElementById('evaAppSecret').value;

            if (enabled && (!serialNumber || !appId)) {
                showEvaConfigStatus('Please enter Serial Number and App ID', false);
                return;
            }

            const formData = new FormData();
            formData.append('enabled', enabled ? 'true' : 'false');
            formData.append('serialNumber', serialNumber);
            formData.append('appId', appId);
            if (appSecret) formData.append('appSecret', appSecret);

            try {
                const response = await fetch('/api/eva', {
                    method: 'POST',
                    body: formData
                });
                const data = await response.json();

                if (data.success) {
                    showEvaConfigStatus('AlphaESS settings saved!', true);
                    document.getElementById('evaAppSecret').value = '';
                    setTimeout(() => location.reload(), 2000);
                } else {
                    showEvaConfigStatus('Error: ' + (data.error || 'Unknown error'), false);
                }
            } catch (error) {
                showEvaConfigStatus('Error saving EVA config: ' + error, false);
            }
        }

        function showEvaConfigStatus(message, success) {
            const statusDiv = document.getElementById('evaConfigStatus');
            statusDiv.textContent = message;
            statusDiv.style.display = 'block';
            statusDiv.style.background = success ? '#d4edda' : '#f8d7da';
            statusDiv.style.color = success ? '#155724' : '#721c24';
        }

        loadConfig();
        loadMqttConfig();
        loadAdvancedConfig();
        loadEvaConfig();
    </script>
</body>
</html>
)rawliteral";

  return html;
}
