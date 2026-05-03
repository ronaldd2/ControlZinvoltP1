/*
 * ActualsPage.cpp - Real-time data display page implementation
 */

#include "ActualsPage.h"
#include "Version.h"

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
        .trace-good {
            color: #155724;
        }
        .trace-warn {
            color: #856404;
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

String getActualsPage() {
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
                <a class="page-link" href="/update">Update</a>
            </div>
        </div>

        <div class="card">
            <h2>📊 Real-time Smart Meter Data</h2>
            <div class="grid">
                <div class="metric">
                    <div class="metric-label">Total Power</div>
                    <div class="metric-value" id="totalPower">0<span class="metric-unit">W</span></div>
                    <div id="totalApiMeterDiv" style="font-size: 0.85em; color: #666; margin-top: 4px;">API meter: 0 W</div>
                    <div id="optimizeSetpointDiv" style="font-size: 0.85em; color: #0c5460; margin-top: 4px;">Setpoint: 0 W</div>
                    <div id="totalModifiedDiv" style="font-size: 0.85em; color: #856404; margin-top: 4px; display: none;">
                        Modified: <strong id="totalModifiedPower">0</strong> W
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L1 Power</div>
                    <div class="metric-value" id="powerL1">0<span class="metric-unit">W</span></div>
                    <div id="modifiedL1Div" style="font-size: 0.85em; color: #856404; margin-top: 4px; display: none;">
                        Modified: <strong id="modifiedPowerL1">0</strong> W
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L2 Power</div>
                    <div class="metric-value" id="powerL2">0<span class="metric-unit">W</span></div>
                    <div id="modifiedL2Div" style="font-size: 0.85em; color: #856404; margin-top: 4px; display: none;">
                        Modified: <strong id="modifiedPowerL2">0</strong> W
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">Phase L3 Power</div>
                    <div class="metric-value" id="powerL3">0<span class="metric-unit">W</span></div>
                    <div id="modifiedL3Div" style="font-size: 0.85em; color: #856404; margin-top: 4px; display: none;">
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

            <div id="optimizeDebugPanel" style="margin-top: 12px; padding: 12px; background: #f0f0f0; border-radius: 6px; font-size: 0.85em; display: none;">
                <details>
                    <summary style="cursor: pointer; font-weight: 500; color: #666;">🧪 Optimize Debug Trace</summary>
                    <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 10px; margin-top: 10px;">
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Actual</span><br><strong id="optTraceActual">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Scaled Actual</span><br><strong id="optTraceScaledActual">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Setpoint</span><br><strong id="optTraceSetpoint">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Error</span><br><strong id="optTraceError">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Command</span><br><strong id="optTraceCommand">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Target</span><br><strong id="optTraceTarget">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Filtered</span><br><strong id="optTraceFiltered">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Integrator</span><br><strong id="optTraceIntegrator">-</strong> W</div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Adjust Divisor</span><br><strong id="optTraceAdjustDiv">-</strong></div>
                        <div style="padding: 8px; background: white; border-radius: 4px;"><span style="color:#888;">Neutral Hold</span><br><strong id="optTraceHold">-</strong></div>
                    </div>
                </details>
            </div>
        </div>

        <div class="card">
            <h2>🔋 Battery Status</h2>
            <div class="grid">
                <div class="metric">
                    <div class="metric-label">Battery SOC</div>
                    <div class="metric-value" id="batterySOC">-<span class="metric-unit">%</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Battery Power</div>
                    <div class="metric-value" id="gridPower">-<span class="metric-unit">W</span></div>
                    <div id="gridPowerApi" style="display: none; font-size: 0.78em; color: #666; margin-top: 4px;"></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Grid Power</div>
                    <div class="metric-value" style="font-size: 1.0em;" id="batteryPower">-<span class="metric-unit">W</span></div>
                    <div id="batteryPowerApi" style="display: none; font-size: 0.78em; color: #666; margin-top: 4px;"></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Solar Power</div>
                    <div class="metric-value" id="solarPower">-<span class="metric-unit">W</span></div>
                    <div id="solarPowerApi" style="display: none; font-size: 0.78em; color: #666; margin-top: 4px;"></div>
                </div>
            </div>
            <div id="evaDisabled" style="display: none; margin-top: 10px; padding: 10px; background: #f8f9fa; border-radius: 6px; color: #666; font-size: 0.9em;">
                Battery API integration is disabled. Enable it in <a href="/settings" style="color: #667eea;">Settings</a>.
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
                    <div class="metric-label">P1 Connection</div>
                    <div class="metric-value">
                        <span id="p1ConnectionStatus" class="status-badge status-inactive">Checking...</span>
                    </div>
                </div>
                <div class="metric">
                    <div class="metric-label">DSMR Version</div>
                    <div class="metric-value" style="font-size: 1.2em;" id="dsmrVersion">-</div>
                </div>
                <div class="metric">
                    <div class="metric-label">P1 Interval</div>
                    <div class="metric-value" id="p1Interval">-<span class="metric-unit">s</span></div>
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
        function formatUptime(seconds) {
            if (!seconds || seconds < 0) return '0s';
            
            const years = Math.floor(seconds / 31536000);
            seconds %= 31536000;
            const days = Math.floor(seconds / 86400);
            seconds %= 86400;
            const hours = Math.floor(seconds / 3600);
            seconds %= 3600;
            const minutes = Math.floor(seconds / 60);
            seconds = Math.floor(seconds % 60);
            
            const parts = [];
            if (years > 0) parts.push(years + 'y');
            if (days > 0) parts.push(days + 'd');
            if (hours > 0) parts.push(hours + 'h');
            if (minutes > 0) parts.push(minutes + 'm');
            if (seconds > 0 || parts.length === 0) parts.push(seconds + 's');
            
            return parts.join(' ');
        }
        
        async function fetchData() {
            try {
                let actualP1TotalW = null;
                const p1Response = await fetch('/api/p1data');
                const p1Data = await p1Response.json();

                if (p1Data.valid) {
                    const power = p1Data.power || {};
                    const modPower = p1Data.modifier || {};
                    const energy = p1Data.energy || {};
                    const current = p1Data.current || { l1: 0, l2: 0, l3: 0 };
                    const voltage = p1Data.voltage || {};

                    const originalTotal = Math.round((power.total || 0) * 1000);
                    actualP1TotalW = originalTotal;
                    const originalL1 = Math.round((power.l1 || 0) * 1000);
                    const originalL2 = Math.round((power.l2 || 0) * 1000);
                    const originalL3 = Math.round((power.l3 || 0) * 1000);
                    
                    const modifiedTotal = Math.round((modPower.total || 0) * 1000);
                    const modifiedL1 = Math.round((modPower.l1 || 0) * 1000);
                    const modifiedL2 = Math.round((modPower.l2 || 0) * 1000);
                    const modifiedL3 = Math.round((modPower.l3 || 0) * 1000);
                    
                    document.getElementById('totalPower').innerHTML = originalTotal + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL1').innerHTML = originalL1 + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL2').innerHTML = originalL2 + '<span class="metric-unit">W</span>';
                    document.getElementById('powerL3').innerHTML = originalL3 + '<span class="metric-unit">W</span>';

                    // Show modified values only if different from original
                    document.getElementById('totalModifiedPower').textContent = modifiedTotal;
                    document.getElementById('totalModifiedDiv').style.display = (modifiedTotal !== originalTotal) ? 'block' : 'none';
                    
                    document.getElementById('modifiedPowerL1').textContent = modifiedL1;
                    document.getElementById('modifiedL1Div').style.display = (modifiedL1 !== originalL1) ? 'block' : 'none';
                    
                    document.getElementById('modifiedPowerL2').textContent = modifiedL2;
                    document.getElementById('modifiedL2Div').style.display = (modifiedL2 !== originalL2) ? 'block' : 'none';
                    
                    document.getElementById('modifiedPowerL3').textContent = modifiedL3;
                    document.getElementById('modifiedL3Div').style.display = (modifiedL3 !== originalL3) ? 'block' : 'none';

                    const todayImport = (energy.todayImport != null) ? energy.todayImport : (energy.import || 0);
                    const todayExport = (energy.todayExport != null) ? energy.todayExport : (energy.export || 0);
                    document.getElementById('energyImport').innerHTML = todayImport.toFixed(3) + '<span class="metric-unit">kWh</span>';
                    document.getElementById('energyExport').innerHTML = todayExport.toFixed(3) + '<span class="metric-unit">kWh</span>';

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
                const p1 = statusData.p1 || {};
                const optimizeCfg = statusData.optimizeConfig || {};
                document.getElementById('currentMode').textContent = modifier.modeString || '—';

                const optimizeSetpointEl = document.getElementById('optimizeSetpointDiv');
                const optimizeDebugPanel = document.getElementById('optimizeDebugPanel');
                if (typeof modifier.optimizeSetpoint === 'number') {
                    optimizeSetpointEl.style.display = 'block';
                    optimizeSetpointEl.textContent = 'Setpoint: ' + Math.round(modifier.optimizeSetpoint) + ' W';

                    const trace = modifier.optimizeTrace || {};
                    optimizeDebugPanel.style.display = (modifier.mode === 8) ? 'block' : 'none';
                    document.getElementById('optTraceActual').textContent = Math.round(trace.actualW || 0);
                    document.getElementById('optTraceScaledActual').textContent = Math.round(trace.scaledActualW || 0);
                    document.getElementById('optTraceSetpoint').textContent = Math.round(modifier.optimizeSetpoint || 0);
                    document.getElementById('optTraceError').textContent = Math.round(trace.errorW || 0);
                    document.getElementById('optTraceCommand').textContent = Math.round(trace.commandW || 0);
                    document.getElementById('optTraceTarget').textContent = Math.round(trace.targetW || 0);
                    document.getElementById('optTraceFiltered').textContent = Math.round(trace.filteredW || 0);
                    document.getElementById('optTraceIntegrator').textContent = (trace.integratorW || 0).toFixed(2);
                    document.getElementById('optTraceAdjustDiv').textContent = (trace.adjustDivisor || 1).toFixed(2);
                    document.getElementById('optTraceHold').textContent = trace.neutralHold ? 'Yes' : 'No';

                    const tolLow = (typeof optimizeCfg.toleranceLow === 'number') ? optimizeCfg.toleranceLow : -5;
                    const tolHigh = (typeof optimizeCfg.toleranceHigh === 'number') ? optimizeCfg.toleranceHigh : 15;
                    const errorValue = (typeof trace.errorW === 'number') ? trace.errorW : 0;
                    const errorInBand = errorValue >= tolLow && errorValue <= tolHigh;

                    const errorEl = document.getElementById('optTraceError');
                    errorEl.classList.remove('trace-good', 'trace-warn');
                    errorEl.classList.add(errorInBand ? 'trace-good' : 'trace-warn');

                    const holdEl = document.getElementById('optTraceHold');
                    holdEl.classList.remove('trace-good', 'trace-warn');
                    holdEl.classList.add(trace.neutralHold ? 'trace-good' : 'trace-warn');
                } else {
                    optimizeSetpointEl.style.display = 'block';
                    optimizeSetpointEl.textContent = 'Setpoint: 0 W';
                    optimizeDebugPanel.style.display = 'none';
                }
                
                document.getElementById('uptime').textContent = formatUptime(statusData.uptime || 0);
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

                const p1StatusEl = document.getElementById('p1ConnectionStatus');
                if (p1.connected) {
                    p1StatusEl.textContent = '✓ Connected';
                    p1StatusEl.className = 'status-badge status-active';
                } else {
                    p1StatusEl.textContent = '✗ Disconnected';
                    p1StatusEl.className = 'status-badge status-inactive';
                }
                
                document.getElementById('dsmrVersion').textContent = p1.dsmrVersion || '-';
                document.getElementById('p1Interval').innerHTML = (p1.interval ? p1.interval.toFixed(0) : '-') + '<span class="metric-unit">s</span>';

                // Update battery data
                if (statusData.battery) {
                    document.getElementById('evaDisabled').style.display = statusData.battery.enabled ? 'none' : 'block';
                    document.getElementById('batterySOC').innerHTML = (statusData.battery.soc || 0) + '<span class="metric-unit">%</span>';
                    const batteryPower = statusData.battery.power || 0;
                    const gridPower = statusData.battery.gridPower || 0;
                    const solarPower = statusData.battery.solarPower || 0;
                    const apiSolarPower = statusData.battery.apiSolarPower || 0;
                    const apiCocPower = statusData.battery.apiCocPower || 0;
                    const apiMeterPower = statusData.battery.apiMeterPower || 0;
                    const powerSign = batteryPower > 10 ? '⚡ Discharging' : batteryPower < -10 ? '🔋 Charging' : '🔌 Standby' ;

                    const totalApiMeterEl = document.getElementById('totalApiMeterDiv');
                    totalApiMeterEl.style.display = 'block';
                    totalApiMeterEl.textContent = 'API meter: ' + Math.round(apiMeterPower) + ' W';

                    document.getElementById('solarPower').innerHTML = solarPower + '<span class="metric-unit">W</span>';
                    const apiSolarEl = document.getElementById('solarPowerApi');
                    if (Math.abs(apiSolarPower) > 0.01) {
                        apiSolarEl.style.display = 'block';
                        apiSolarEl.textContent = 'API: ' + apiSolarPower + ' W';
                    } else {
                        apiSolarEl.style.display = 'none';
                        apiSolarEl.textContent = '';
                    }


                    let efficiencyText = '';
                    const absBatt = Math.abs(batteryPower);
                    const absGrid = Math.abs(gridPower);
                    if (absBatt > 1 && absGrid > 1) {
                        // Always calculate as smaller/larger to keep efficiency <= 100%
                        // Charging: battery stored / grid consumed (always < 100%)
                        // Discharging: grid delivered / battery used (always < 100%)
                        const eff = Math.min(100.0, (Math.min(absBatt, absGrid) / Math.max(absBatt, absGrid)) * 100.0);
                        efficiencyText = ' (' + eff.toFixed(0) + '%)';
                    }
                    document.getElementById('gridPower').innerHTML = gridPower + '<span class="metric-unit">W</span>'+ powerSign;
                    document.getElementById('batteryPower').innerHTML = Math.abs(batteryPower) + '<span class="metric-unit">W</span> '  + efficiencyText;

                    const cocEl = document.getElementById('batteryPowerApi');
                    if (Math.abs(apiCocPower) > 0.0001) {
                        cocEl.style.display = 'block';
                        cocEl.textContent = 'CoC: ' + apiCocPower.toFixed(3) + ' kWh';
                    } else {
                        cocEl.style.display = 'none';
                        cocEl.textContent = '';
                    }

                    const meterEl = document.getElementById('gridPowerApi');
                    meterEl.style.display = 'none';
                    meterEl.textContent = '';
                } else {
                    document.getElementById('evaDisabled').style.display = 'block';
                    document.getElementById('batterySOC').innerHTML = '-<span class="metric-unit">%</span>';
                    document.getElementById('batteryPower').innerHTML = '-<span class="metric-unit">W</span>';
                    document.getElementById('gridPower').innerHTML = '-<span class="metric-unit">W</span>';
                    document.getElementById('totalApiMeterDiv').style.display = 'block';
                    document.getElementById('totalApiMeterDiv').textContent = 'API meter: 0 W';
                    document.getElementById('solarPowerApi').style.display = 'none';
                    document.getElementById('solarPowerApi').textContent = '';
                    document.getElementById('batteryPowerApi').style.display = 'none';
                    document.getElementById('batteryPowerApi').textContent = '';
                    document.getElementById('gridPowerApi').style.display = 'none';
                    document.getElementById('gridPowerApi').textContent = '';
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
