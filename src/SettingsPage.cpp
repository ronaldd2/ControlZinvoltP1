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
                    <option value="1">Off</option>
                    <option value="2">Force Charge</option>
                    <option value="3">Force Discharge</option>
                    <option value="4">Charge Only</option>
                    <option value="5">Discharge Only</option>
                    <option value="6">External Control</option>
                    <option value="7">Self-Use Limiter</option>
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
            <h2>🌱 Self-Use Limiter Mode</h2>
            <p style="color: #666; font-size: 0.9em; margin-bottom: 20px;">Limit self-consumption by delivering extra power to the grid when battery/solar is active. Uses exponential smoothing to prevent oscillations from P1 meter updates and battery cloud lag.</p>

            <div class="control-group">
                <label>Export Threshold (Watts) <span style="color: #667eea; font-size: 0.85em;">Default: 20W</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">Extra power to show as grid delivery when battery is active</small>
                <input type="number" id="selfUseThreshold" min="0" max="1000" step="1" value="20">
            </div>

            <div class="control-group">
                <label>Smoothing Factor <span style="color: #667eea; font-size: 0.85em;">Default: 0.3</span></label>
                <small style="color: #666; display: block; margin-bottom: 8px;">0.1 (max smoothing) to 1.0 (no smoothing). Lower = slower response, prevents oscillation.</small>
                <input type="range" id="selfUseSmoothingSlider" min="0.1" max="1.0" step="0.05" value="0.3" oninput="updateSmoothingDisplay(this.value)">
                <div style="margin-top: 8px; display: flex; justify-content: space-between;">
                    <span id="smoothingDisplay" style="font-weight: bold; color: #667eea;">0.30</span>
                    <span style="font-size: 0.85em; color: #999;">← More Smoothing | Less Smoothing →</span>
                </div>
            </div>

            <div style="display: flex; gap: 10px; flex-wrap: wrap;">
                <button onclick="saveSelfUseConfig()">Save Self-Use Limiter Settings</button>
                <button style="background: #f0f0f0; color: #444;" onclick="resetSelfUseDefaults()">Reset to Defaults</button>
            </div>

            <div id="selfUseConfigStatus" style="margin-top: 15px; padding: 10px; border-radius: 6px; display: none;"></div>
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

        async function loadSelfUseConfig() {
            try {
                const response = await fetch('/api/selfuse');
                const data = await response.json();
                document.getElementById('selfUseThreshold').value = data.threshold || 20;
                document.getElementById('selfUseSmoothingSlider').value = data.smoothingFactor || 0.3;
                updateSmoothingDisplay(data.smoothingFactor || 0.3);
            } catch (error) {
                console.error('Error fetching self-use config:', error);
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

        function updateSmoothingDisplay(value) {
            document.getElementById('smoothingDisplay').textContent = parseFloat(value).toFixed(2);
        }

        async function saveSelfUseConfig() {
            const threshold = document.getElementById('selfUseThreshold').value;
            const smoothingFactor = document.getElementById('selfUseSmoothingSlider').value;

            const formData = new FormData();
            formData.append('threshold', threshold);
            formData.append('smoothingFactor', smoothingFactor);

            try {
                const response = await fetch('/api/selfuse', {
                    method: 'POST',
                    body: formData
                });
                const data = await response.json();

                if (data.success) {
                    showSelfUseConfigStatus(data.message || 'Settings saved successfully!', true);
                    loadConfig();
                } else {
                    showSelfUseConfigStatus('Error: ' + (data.error || 'Unknown error'), false);
                }
            } catch (error) {
                showSelfUseConfigStatus('Error saving self-use config: ' + error, false);
            }
        }

        function showSelfUseConfigStatus(message, success) {
            const statusDiv = document.getElementById('selfUseConfigStatus');
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
        loadSelfUseConfig();
        loadEvaConfig();
    </script>
</body>
</html>
)rawliteral";

  return html;
}
