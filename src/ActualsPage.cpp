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
                    <div class="metric-label">Grid Power</div>
                    <div class="metric-value" id="gridPower">-<span class="metric-unit">W</span></div>
                </div>
                <div class="metric">
                    <div class="metric-label">Battery Power</div>
                    <div class="metric-value" style="font-size: 1.0em;" id="batteryPower">-<span class="metric-unit">W</span></div>
                </div>
            </div>
            <div id="evaDisabled" style="display: none; margin-top: 10px; padding: 10px; background: #f8f9fa; border-radius: 6px; color: #666; font-size: 0.9em;">
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
                const p1 = statusData.p1 || {};
                document.getElementById('currentMode').textContent = modifier.modeString || '—';
                
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
                document.getElementById('p1Interval').innerHTML = (p1.interval ? p1.interval.toFixed(1) : '-') + '<span class="metric-unit">s</span>';

                // Update battery data
                if (statusData.battery) {
                    document.getElementById('batterySOC').innerHTML = (statusData.battery.soc || 0) + '<span class="metric-unit">%</span>';
                    const batteryPower = statusData.battery.power || 0;
                    const gridPower = statusData.battery.gridPower || 0;
                    const powerSign = batteryPower > 10 ? '⚡ Discharging' : batteryPower < -10 ? '🔋 Charging' : '🔌 Standby' ;


                    let efficiencyText = '';
                    const absBatt = Math.abs(batteryPower);
                    const absGrid = Math.abs(gridPower);
                    if (absBatt > 1 && absGrid > 1) {
                        const eff = Math.min(999.0, (absGrid / absBatt) * 100.0);
                        efficiencyText = ' (' + eff.toFixed(0) + '%)';
                    }
                    document.getElementById('gridPower').innerHTML = gridPower + '<span class="metric-unit">W</span>'+ powerSign;
                    document.getElementById('batteryPower').innerHTML = Math.abs(batteryPower) + '<span class="metric-unit">W</span> '  + efficiencyText;
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
