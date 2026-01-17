/*
 * WebInterface.cpp - Implementation of web server and REST API
 */

#include "WebInterface.h"

WebInterface::WebInterface(AsyncWebServer* server, P1Parser* parser, P1Modifier* modifier) {
  _server = server;
  _parser = parser;
  _modifier = modifier;
}

void WebInterface::begin() {
  // Serve main page
  _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleRoot(request);
  });
  
  // REST API endpoints
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
  
  // 404 handler
  _server->onNotFound([this](AsyncWebServerRequest* request) {
    handleNotFound(request);
  });
  
  Serial.println("Web interface routes configured");
}

void WebInterface::handleRoot(AsyncWebServerRequest* request) {
  request->send(200, "text/html", getHTMLPage());
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
  power["total"] = _parser->getTotalActivePower();
  power["l1"] = _parser->getActivePowerL1();
  power["l2"] = _parser->getActivePowerL2();
  power["l3"] = _parser->getActivePowerL3();
  
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
  energy["import"] = _parser->getTotalEnergyImport();
  energy["export"] = _parser->getTotalEnergyExport();
  
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

void WebInterface::handleNotFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

String WebInterface::getStatusJSON() {
  JsonDocument doc;
  
  // System info
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["chipModel"] = ESP.getChipModel();
  
  // WiFi info
  doc["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["ssid"] = WiFi.SSID();
  doc["wifi"]["ip"] = WiFi.localIP().toString();
  doc["wifi"]["rssi"] = WiFi.RSSI();
  
  // P1 parser status
  doc["p1"]["valid"] = _parser->isValid();
  doc["p1"]["totalPower"] = _parser->getTotalActivePower();
  
  // Modifier status
  doc["modifier"]["mode"] = _modifier->getMode();
  doc["modifier"]["modeString"] = _modifier->getModeString();
  doc["modifier"]["batteryPhase"] = _modifier->getBatteryPhase();
  doc["modifier"]["modifyPhase"] = _modifier->getModifyPhase();
  doc["modifier"]["forcePower"] = _modifier->getForcePower();
  
  String response;
  serializeJson(doc, response);
  return response;
}

String WebInterface::getHTMLPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ControlZinvoltP1</title>
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
        .card {
            background: white;
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 8px 16px rgba(0,0,0,0.1);
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
        select, input {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 1em;
            transition: border-color 0.3s;
        }
        select:focus, input:focus {
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
            margin-top: 10px;
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
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>⚡ ControlZinvoltP1</h1>
            <p>ESP32-S3 P1 Controller for Zinvolt VT1000</p>
        </div>

        <div class="card">
            <h2>📊 Real-time Smart Meter Data</h2>
            <div class="grid">
                <div class="metric">
                    <div class="metric-label">Total Power</div>
                    <div class="metric-value" id="totalPower">0<span class="metric-unit">W</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L1</div>
                    <div class="metric-value" id="powerL1">0<span class="metric-unit">W</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L2</div>
                    <div class="metric-value" id="powerL2">0<span class="metric-unit">W</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L3</div>
                    <div class="metric-value" id="powerL3">0<span class="metric-unit">W</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Voltage L1</div>
                    <div class="metric-value" id="voltageL1">0<span class="metric-unit">V</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Voltage L2</div>
                    <div class="metric-value" id="voltageL2">0<span class="metric-unit">V</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Voltage L3</div>
                    <div class="metric-value" id="voltageL3">0<span class="metric-unit">V</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Current Mode</div>
                    <div class="metric-value" style="font-size: 1.2em;" id="currentMode">Loading...</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>🎛️ Control Settings</h2>
            
            <div class="control-group">
                <label>Operation Mode</label>
                <select id="modeSelect">
                    <option value="0">Unmodified Forward</option>
                    <option value="1">Off (Prevent Charging/Discharging)</option>
                    <option value="2">Force Charge</option>
                    <option value="3">Force Discharge</option>
                    <option value="4">Charge Only (Future)</option>
                    <option value="5">Discharge Only (Future)</option>
                </select>
                <button onclick="setMode()">Set Mode</button>
            </div>

            <div class="control-group">
                <label>Force Power (Watts)</label>
                <input type="number" id="forcePower" min="0" max="20000" step="100" value="3000">
                <button onclick="setPower()">Set Power</button>
            </div>

            <div class="control-group">
                <label>Battery Phase</label>
                <div class="phase-selector">
                    <button class="phase-btn" id="battPhase1" onclick="setBatteryPhase(1)">L1</button>
                    <button class="phase-btn" id="battPhase2" onclick="setBatteryPhase(2)">L2</button>
                    <button class="phase-btn" id="battPhase3" onclick="setBatteryPhase(3)">L3</button>
                </div>
            </div>

            <div class="control-group">
                <label>Modify Phase</label>
                <div class="phase-selector">
                    <button class="phase-btn" id="modPhase1" onclick="setModifyPhase(1)">L1</button>
                    <button class="phase-btn" id="modPhase2" onclick="setModifyPhase(2)">L2</button>
                    <button class="phase-btn" id="modPhase3" onclick="setModifyPhase(3)">L3</button>
                </div>
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
            </div>
        </div>
    </div>

    <script>
        let currentBatteryPhase = 1;
        let currentModifyPhase = 1;

        async function fetchData() {
            try {
                // Fetch P1 data
                const p1Response = await fetch('/api/p1data');
                const p1Data = await p1Response.json();
                
                if (p1Data.valid) {
                    document.getElementById('totalPower').innerHTML = Math.round(p1Data.power.total * 1000) + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL1').innerHTML = Math.round(p1Data.power.l1 * 1000) + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL2').innerHTML = Math.round(p1Data.power.l2 * 1000) + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL3').innerHTML = Math.round(p1Data.power.l3 * 1000) + '<span class="metric-unit">W</span>';
                    
                    document.getElementById('voltageL1').innerHTML = p1Data.voltage.l1.toFixed(1) + '<span class="metric-unit">V</span>';
                    document.getElementById('voltageL2').innerHTML = p1Data.voltage.l2.toFixed(1) + '<span class="metric-unit">V</span>';
                    document.getElementById('voltageL3').innerHTML = p1Data.voltage.l3.toFixed(1) + '<span class="metric-unit">V</span>';
                }

                // Fetch status
                const statusResponse = await fetch('/api/status');
                const statusData = await statusResponse.json();
                
                document.getElementById('currentMode').textContent = statusData.modifier.modeString;
                document.getElementById('uptime').innerHTML = statusData.uptime + '<span class="metric-unit">s</span>';
                document.getElementById('wifiSSID').textContent = statusData.wifi.ssid;
                document.getElementById('ipAddress').textContent = statusData.wifi.ip;
                document.getElementById('wifiRSSI').innerHTML = statusData.wifi.rssi + '<span class="metric-unit">dBm</span>';
                
                // Fetch config
                const configResponse = await fetch('/api/config');
                const configData = await configResponse.json();
                
                document.getElementById('modeSelect').value = configData.mode;
                document.getElementById('forcePower').value = configData.forcePower;
                
                currentBatteryPhase = configData.batteryPhase;
                currentModifyPhase = configData.modifyPhase;
                
                updatePhaseButtons();
            } catch (error) {
                console.error('Error fetching data:', error);
            }
        }

        function updatePhaseButtons() {
            // Update battery phase buttons
            for (let i = 1; i <= 3; i++) {
                const btn = document.getElementById('battPhase' + i);
                if (i === currentBatteryPhase) {
                    btn.classList.add('active');
                } else {
                    btn.classList.remove('active');
                }
            }
            
            // Update modify phase buttons
            for (let i = 1; i <= 3; i++) {
                const btn = document.getElementById('modPhase' + i);
                if (i === currentModifyPhase) {
                    btn.classList.add('active');
                } else {
                    btn.classList.remove('active');
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
                    fetchData();
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

        // Update data every 2 seconds
        fetchData();
        setInterval(fetchData, 2000);
    </script>
</body>
</html>
)rawliteral";

  return html;
}
