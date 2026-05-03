/*
 * SettingsPage.cpp - Device configuration page implementation
 */

#include "SettingsPage.h"

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
            .page-links {
                flex-direction: column;
            }
        }
    </style>
)rawliteral";

String getSettingsPage() {
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
                <a class="page-link" href="/update">Update</a>
            </div>
        </div>

        <div class="card">
            <h2>🎛️ Control Settings</h2>
            <p style="color: #666; font-size: 0.9em; margin-bottom: 20px;">These settings control P1 telegram modification and battery behavior. Changes are also published to MQTT/Home Assistant.</p>

            <div class="control-group">
                <label>Operation Mode <span style="color: #667eea; font-size: 0.85em;">(MQTT: operation_mode)</span></label>
                <select id="modeSelect">
                    <option value="0">Unmodified Forward</option>
                    <option value="1">Battery Off</option>
                    <option value="2">Force Charge</option>
                    <option value="3">Force Discharge</option>
                    <option value="4">Power Control</option>
                    <option value="5">Charge Only</option>
                    <option value="6">Discharge Only</option>
                    <option value="7">External Control</option>
                    <option value="8">Optimize</option>
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

            <div class="control-group">
                <label style="display: flex; align-items: center; gap: 10px;">
                    <input type="checkbox" id="singlePhaseMode" style="width: auto;">
                    <span>Transform meter to single phase (L2/L3 = 0)</span>
                </label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Keeps total power correct by mapping all power to L1 in modified telegrams.</small>
                <button onclick="setSinglePhaseMode()">Apply Single-Phase Setting</button>
            </div>
        </div>

        <div class="card">
            <h2>🌱 Optimize Mode Settings</h2>
            <p style="color: #666; font-size: 0.9em; margin-bottom: 20px;">Configure the Domoticz-based PI control algorithm for smart battery management. Adjusts grid delivery based on solar production and battery activity.</p>

            <h3 style="color: #667eea; font-size: 1.1em; margin: 20px 0 10px 0; border-bottom: 1px solid #eee; padding-bottom: 5px;">Target Setpoints</h3>
            
            <div class="control-group">
                <label>Delivery Setpoint (W) <span style="color: #667eea; font-size: 0.85em;">Default: 20W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Target grid delivery power in watts</small>
                <input type="number" id="optDelSetpt" min="0" max="500" step="1" value="20">
            </div>

            <div class="control-group">
                <label>High Solar Setpoint (W) <span style="color: #667eea; font-size: 0.85em;">Default: 100W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Setpoint when solar exceeds threshold</small>
                <input type="number" id="optHiSolSet" min="0" max="500" step="1" value="100">
            </div>

            <h3 style="color: #667eea; font-size: 1.1em; margin: 20px 0 10px 0; border-bottom: 1px solid #eee; padding-bottom: 5px;">Thresholds</h3>
            
            <div class="control-group">
                <label>Min EVA Activity (W) <span style="color: #667eea; font-size: 0.85em;">Default: 10W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Minimum charge/discharge power to consider battery active</small>
                <input type="number" id="optMinEva" min="0" max="100" step="1" value="10">
            </div>

            <div class="control-group">
                <label>Min Solar Power (W) <span style="color: #667eea; font-size: 0.85em;">Default: 20W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Minimum solar power threshold</small>
                <input type="number" id="optMinSolar" min="0" max="100" step="1" value="20">
            </div>

            <div class="control-group">
                <label>Solar Threshold (W) <span style="color: #667eea; font-size: 0.85em;">Default: 300W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">High solar power threshold for setpoint switching</small>
                <input type="number" id="optSolarThr" min="100" max="1000" step="10" value="300">
            </div>

            <div class="control-group">
                <label>Filter Time Constant (s) <span style="color: #667eea; font-size: 0.85em;">Default: 45s</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">First-order filter on measured grid delivery, updated each telegram interval</small>
                <input type="number" id="optFilterTau" min="1" max="300" step="1" value="45">
            </div>

            <h3 style="color: #667eea; font-size: 1.1em; margin: 20px 0 10px 0; border-bottom: 1px solid #eee; padding-bottom: 5px;">Control Parameters</h3>
            
            <div class="control-group">
                <label>Min Delivery for Adjust (W) <span style="color: #667eea; font-size: 0.85em;">Default: 60W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Minimum delivery for full adjustment (below uses divisor)</small>
                <input type="number" id="optMinDelAdj" min="0" max="200" step="5" value="60">
            </div>

            <div class="control-group">
                <label>Adjust Divisor <span style="color: #667eea; font-size: 0.85em;">Default: 3.0</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Divisor for slow adjustment when below min delivery</small>
                <input type="number" id="optAdjDiv" min="1" max="10" step="0.5" value="3.0">
            </div>

            <div class="control-group">
                <label>Tolerance Low (W) <span style="color: #667eea; font-size: 0.85em;">Default: -5W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Lower tolerance band (negative value)</small>
                <input type="number" id="optTolLow" min="-50" max="0" step="1" value="-5">
            </div>

            <div class="control-group">
                <label>Tolerance High (W) <span style="color: #667eea; font-size: 0.85em;">Default: 15W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Upper tolerance band</small>
                <input type="number" id="optTolHigh" min="0" max="100" step="1" value="15">
            </div>

            <h3 style="color: #667eea; font-size: 1.1em; margin: 20px 0 10px 0; border-bottom: 1px solid #eee; padding-bottom: 5px;">Integrator Settings</h3>
            
            <div class="control-group">
                <label>Large Error Threshold (W) <span style="color: #667eea; font-size: 0.85em;">Default: 200W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Error threshold for integrator reduction</small>
                <input type="number" id="optLrgErrThr" min="50" max="500" step="10" value="200">
            </div>

            <div class="control-group">
                <label>Integrator Reduction <span style="color: #667eea; font-size: 0.85em;">Default: 0.66</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Factor to reduce integrator on large error (0-1)</small>
                <input type="number" id="optIntRed" min="0.1" max="1.0" step="0.01" value="0.66">
            </div>

            <div class="control-group">
                <label>Integrator Min <span style="color: #667eea; font-size: 0.85em;">Default: -10</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Minimum integrator clamp value</small>
                <input type="number" id="optIntMin" min="-50" max="0" step="1" value="-10">
            </div>

            <div class="control-group">
                <label>Integrator Max <span style="color: #667eea; font-size: 0.85em;">Default: 10</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Maximum integrator clamp value</small>
                <input type="number" id="optIntMax" min="0" max="50" step="1" value="10">
            </div>

            <div class="control-group">
                <label>Integrator Step <span style="color: #667eea; font-size: 0.85em;">Default: 1.0</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Increment/decrement step per minute</small>
                <input type="number" id="optIntStep" min="0.1" max="5.0" step="0.1" value="1.0">
            </div>

            <div class="control-group">
                <label>Error Deadband (W) <span style="color: #667eea; font-size: 0.85em;">Default: 5W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Error deadband for integrator update</small>
                <input type="number" id="optErrDead" min="0" max="50" step="1" value="5">
            </div>

            <h3 style="color: #667eea; font-size: 1.1em; margin: 20px 0 10px 0; border-bottom: 1px solid #eee; padding-bottom: 5px;">Hysteresis</h3>
            
            <div class="control-group">
                <label>Hysteresis Delivery (W) <span style="color: #667eea; font-size: 0.85em;">Default: 40W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Delivery threshold for hysteresis activation</small>
                <input type="number" id="optHysDel" min="0" max="100" step="1" value="40">
            </div>

            <div class="control-group">
                <label>Hysteresis Adjustment (W) <span style="color: #667eea; font-size: 0.85em;">Default: -50W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Adjustment value for hysteresis</small>
                <input type="number" id="optHysAdj" min="-200" max="0" step="5" value="-50">
            </div>

            <div style="display: flex; gap: 10px; flex-wrap: wrap;">
                <button onclick="saveOptimizeConfig()">Save Optimize Settings</button>
                <button style="background: #f0f0f0; color: #444;" onclick="resetOptimizeDefaults()">Reset to Defaults</button>
            </div>

            <div id="optimizeConfigStatus" style="margin-top: 15px; padding: 10px; border-radius: 6px; display: none;"></div>
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
            <h2>🔋 Battery API Integration</h2>
            <p style="color: #666; font-size: 0.9em; margin-bottom: 20px;">Choose your cloud backend (Zinvolt or AlphaESS) to fetch battery data every 10 seconds.</p>

            <div class="control-group">
                <label style="display: flex; align-items: center; gap: 10px;">
                    <input type="checkbox" id="evaEnabled" style="width: auto;">
                    <span>Enable Battery API Integration</span>
                </label>
            </div>

            <div class="control-group">
                <label>Backend</label>
                <select id="batteryBackend" onchange="updateBatteryBackendFields()">
                    <option value="zinvolt">Zinvolt</option>
                    <option value="alphaess">AlphaESS</option>
                </select>
            </div>

            <div id="zinvoltFields" style="display: none;">
                <div class="control-group">
                    <label>Zinvolt Email</label>
                    <input type="text" id="zinvoltEmail" placeholder="you@example.com">
                </div>

                <div class="control-group">
                    <label>Zinvolt Password</label>
                    <input type="password" id="zinvoltPassword" placeholder="Leave empty to keep current">
                </div>

                <div class="control-group">
                    <label>Zinvolt Battery ID (optional)</label>
                    <input type="text" id="zinvoltBatteryId" placeholder="Auto-detect first battery if empty">
                </div>
            </div>

            <div id="alphaessFields" style="display: none;">
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
            </div>

            <button onclick="saveEvaConfig()">Save Battery API Configuration</button>

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
            <button onclick="rebootDevice()" style="background: #dc3545; margin-left: 10px;">Reboot Device</button>

            <div id="advancedStatus" style="margin-top: 15px; padding: 10px; border-radius: 6px; display: none;"></div>
        </div>
    </div>

    <script>
        let currentBatteryPhase = 1;
        let currentModifyPhase = 1;
        let currentSinglePhaseMode = false;

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
                currentSinglePhaseMode = data.singlePhaseMode || false;
                document.getElementById('singlePhaseMode').checked = currentSinglePhaseMode;
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
                if (data.singlePhaseMode != null) {
                    currentSinglePhaseMode = data.singlePhaseMode;
                    document.getElementById('singlePhaseMode').checked = currentSinglePhaseMode;
                }
            } catch (error) {
                console.error('Error fetching advanced config:', error);
            }
        }

        async function loadOptimizeConfig() {
            try {
                const response = await fetch('/api/optimize');
                const data = await response.json();
                document.getElementById('optDelSetpt').value = data.deliverySetpoint || 20;
                document.getElementById('optMinEva').value = data.minEvaActivity || 10;
                document.getElementById('optMinSolar').value = data.minSolarPower || 20;
                document.getElementById('optSolarThr').value = data.solarThreshold || 300;
                document.getElementById('optHiSolSet').value = data.highSolarSetpoint || 100;
                document.getElementById('optFilterTau').value = data.filterTimeConstant || 45;
                document.getElementById('optMinDelAdj').value = data.minDeliveryForAdjust || 60;
                document.getElementById('optAdjDiv').value = data.adjustDivisor || 3.0;
                document.getElementById('optTolLow').value = data.toleranceLow || -5;
                document.getElementById('optTolHigh').value = data.toleranceHigh || 15;
                document.getElementById('optLrgErrThr').value = data.largeErrorThreshold || 200;
                document.getElementById('optIntRed').value = data.integratorReduction || 0.66;
                document.getElementById('optHysDel').value = data.hysteresisDelivery || 40;
                document.getElementById('optHysAdj').value = data.hysteresisAdjustment || -50;
                document.getElementById('optIntMin').value = data.integratorMin || -10;
                document.getElementById('optIntMax').value = data.integratorMax || 10;
                document.getElementById('optIntStep').value = data.integratorStep || 1.0;
                document.getElementById('optErrDead').value = data.errorDeadband || 5;
            } catch (error) {
                console.error('Error fetching optimize config:', error);
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
                    loadConfig();
                }
            } catch (error) {
                console.error('Error setting power:', error);
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
                console.error('Error setting battery phase:', error);
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
                console.error('Error setting modify phase:', error);
            }
        }

        async function setSinglePhaseMode() {
            const enabled = document.getElementById('singlePhaseMode').checked;
            try {
                const response = await fetch('/api/singlephase?enabled=' + (enabled ? '1' : '0'));
                const data = await response.json();
                if (data.success) {
                    currentSinglePhaseMode = enabled;
                }
            } catch (error) {
                console.error('Error setting single phase mode:', error);
            }
        }

        async function saveOptimizeConfig() {
            const formData = new FormData();
            formData.append('deliverySetpoint', document.getElementById('optDelSetpt').value);
            formData.append('minEvaActivity', document.getElementById('optMinEva').value);
            formData.append('minSolarPower', document.getElementById('optMinSolar').value);
            formData.append('solarThreshold', document.getElementById('optSolarThr').value);
            formData.append('highSolarSetpoint', document.getElementById('optHiSolSet').value);
            formData.append('filterTimeConstant', document.getElementById('optFilterTau').value);
            formData.append('minDeliveryForAdjust', document.getElementById('optMinDelAdj').value);
            formData.append('adjustDivisor', document.getElementById('optAdjDiv').value);
            formData.append('toleranceLow', document.getElementById('optTolLow').value);
            formData.append('toleranceHigh', document.getElementById('optTolHigh').value);
            formData.append('largeErrorThreshold', document.getElementById('optLrgErrThr').value);
            formData.append('integratorReduction', document.getElementById('optIntRed').value);
            formData.append('hysteresisDelivery', document.getElementById('optHysDel').value);
            formData.append('hysteresisAdjustment', document.getElementById('optHysAdj').value);
            formData.append('integratorMin', document.getElementById('optIntMin').value);
            formData.append('integratorMax', document.getElementById('optIntMax').value);
            formData.append('integratorStep', document.getElementById('optIntStep').value);
            formData.append('errorDeadband', document.getElementById('optErrDead').value);

            try {
                const response = await fetch('/api/optimize', {
                    method: 'POST',
                    body: formData
                });
                const data = await response.json();

                if (data.success) {
                    showOptimizeConfigStatus(data.message || 'Optimize settings saved successfully!', true);
                    loadConfig();
                } else {
                    showOptimizeConfigStatus('Error: ' + (data.error || 'Unknown error'), false);
                }
            } catch (error) {
                showOptimizeConfigStatus('Error saving optimize config: ' + error, false);
            }
        }

        async function resetOptimizeDefaults() {
            document.getElementById('optDelSetpt').value = 20;
            document.getElementById('optMinEva').value = 10;
            document.getElementById('optMinSolar').value = 20;
            document.getElementById('optSolarThr').value = 300;
            document.getElementById('optHiSolSet').value = 100;
            document.getElementById('optFilterTau').value = 45;
            document.getElementById('optMinDelAdj').value = 60;
            document.getElementById('optAdjDiv').value = 3.0;
            document.getElementById('optTolLow').value = -5;
            document.getElementById('optTolHigh').value = 15;
            document.getElementById('optLrgErrThr').value = 200;
            document.getElementById('optIntRed').value = 0.66;
            document.getElementById('optHysDel').value = 40;
            document.getElementById('optHysAdj').value = -50;
            document.getElementById('optIntMin').value = -10;
            document.getElementById('optIntMax').value = 10;
            document.getElementById('optIntStep').value = 1.0;
            document.getElementById('optErrDead').value = 5;
        }

        function showOptimizeConfigStatus(message, success) {
            const statusDiv = document.getElementById('optimizeConfigStatus');
            statusDiv.textContent = message;
            statusDiv.style.display = 'block';
            statusDiv.style.background = success ? '#d4edda' : '#f8d7da';
            statusDiv.style.color = success ? '#155724' : '#721c24';
        }

        async function rebootDevice() {
            const confirmed = confirm('Reboot device now? Settings are already saved and will not be reset.');
            if (!confirmed) return;

            try {
                const response = await fetch('/api/reboot');
                const data = await response.json();
                if (data.success) {
                    showAdvancedStatus('Rebooting device...', true);
                    setTimeout(() => location.reload(), 4000);
                } else {
                    showAdvancedStatus('Error: ' + (data.error || 'Unknown error'), false);
                }
            } catch (error) {
                showAdvancedStatus('Error sending reboot command: ' + error, false);
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
            const singlePhaseMode = document.getElementById('singlePhaseMode').checked;
            const webUsername = document.getElementById('webUsername').value;
            const webPassword = document.getElementById('webPassword').value;

            if (!webUsername) {
                showAdvancedStatus('Please enter a username', false);
                return;
            }

            const formData = new FormData();
            formData.append('useTxReq', useTxReq ? 'true' : 'false');
            formData.append('singlePhaseMode', singlePhaseMode ? 'true' : 'false');
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

        function updateBatteryBackendFields() {
            const backend = document.getElementById('batteryBackend').value || 'alphaess';
            document.getElementById('zinvoltFields').style.display = backend === 'zinvolt' ? 'block' : 'none';
            document.getElementById('alphaessFields').style.display = backend === 'alphaess' ? 'block' : 'none';
        }

        async function loadEvaConfig() {
            try {
                const response = await fetch('/api/eva');
                const data = await response.json();
                document.getElementById('evaEnabled').checked = data.enabled || false;
                document.getElementById('batteryBackend').value = data.backend || 'alphaess';
                document.getElementById('evaSerialNumber').value = data.serialNumber || '';
                document.getElementById('evaAppId').value = data.appId || '';
                document.getElementById('zinvoltEmail').value = data.zinvoltEmail || '';
                document.getElementById('zinvoltBatteryId').value = data.zinvoltBatteryId || '';
                updateBatteryBackendFields();
            } catch (error) {
                console.error('Error fetching EVA config:', error);
            }
        }

        async function saveEvaConfig() {
            const enabled = document.getElementById('evaEnabled').checked;
            const backend = document.getElementById('batteryBackend').value;
            const serialNumber = document.getElementById('evaSerialNumber').value;
            const appId = document.getElementById('evaAppId').value;
            const appSecret = document.getElementById('evaAppSecret').value;
            const zinvoltEmail = document.getElementById('zinvoltEmail').value;
            const zinvoltPassword = document.getElementById('zinvoltPassword').value;
            const zinvoltBatteryId = document.getElementById('zinvoltBatteryId').value;

            if (enabled) {
                if (backend === 'alphaess' && (!serialNumber || !appId)) {
                    showEvaConfigStatus('Please enter AlphaESS Serial Number and App ID', false);
                    return;
                }
                if (backend === 'zinvolt' && !zinvoltEmail) {
                    showEvaConfigStatus('Please enter Zinvolt email', false);
                    return;
                }
            }

            const formData = new FormData();
            formData.append('enabled', enabled ? 'true' : 'false');
            formData.append('backend', backend);
            formData.append('serialNumber', serialNumber);
            formData.append('appId', appId);
            if (appSecret) formData.append('appSecret', appSecret);
            formData.append('zinvoltEmail', zinvoltEmail);
            formData.append('zinvoltBatteryId', zinvoltBatteryId);
            if (zinvoltPassword) formData.append('zinvoltPassword', zinvoltPassword);

            try {
                const response = await fetch('/api/eva', {
                    method: 'POST',
                    body: formData
                });
                const data = await response.json();

                if (data.success) {
                    showEvaConfigStatus('Battery API settings saved!', true);
                    document.getElementById('evaAppSecret').value = '';
                    document.getElementById('zinvoltPassword').value = '';
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
        loadOptimizeConfig();
        loadEvaConfig();
    </script>
</body>
</html>
)rawliteral";

  return html;
}
