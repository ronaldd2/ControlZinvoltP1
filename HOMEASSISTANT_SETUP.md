# Home Assistant MQTT Integration Guide

## Overview
Your ControlZinvoltP1 device now has full Home Assistant integration via MQTT with auto-discovery. All P1 meter data, battery information, and control settings are automatically exposed to Home Assistant.

## Prerequisites
1. **MQTT Broker** running on your Home Assistant (Mosquitto add-on recommended)
2. **Home Assistant** with MQTT integration enabled

## Setup Instructions

### 1. Install MQTT Broker in Home Assistant
If you haven't already:
1. Go to **Settings → Add-ons → Add-on Store**
2. Search for "Mosquitto broker"
3. Click Install
4. Start the Mosquitto broker add-on
5. Enable "Start on boot"

### 2. Configure MQTT in Home Assistant
1. Go to **Settings → Devices & Services**
2. Click "Add Integration"
3. Search for "MQTT"
4. Use default settings (localhost:1883)
5. Save

### 3. Configure Your ESP32-S3 Device

Edit `src/main.cpp` around line 111 and uncomment/modify one of these lines:

**Option A: No Authentication (simple)**
```cpp
homeAssistant.begin("192.168.1.100", 1883);
```
Replace `192.168.1.100` with your Home Assistant IP address.

**Option B: With Authentication (recommended)**
```cpp
homeAssistant.begin("homeassistant.local", 1883, "mqtt_user", "mqtt_password");
```

To create MQTT user in Home Assistant:
1. Go to **Settings → People → Users tab**
2. Click "Add User"
3. Create username/password for MQTT
4. Use these credentials in the code

### 4. Build and Upload
```bash
pio run --target upload
```

Or use the PlatformIO extension buttons in VS Code.

## What Gets Exposed to Home Assistant

### Sensors (Real-time Data)
- **Total Power** (W) - Combined power from all phases
- **L1/L2/L3 Power** (W) - Power per phase
- **L1/L2/L3 Current** (A) - Current per phase
- **L1/L2/L3 Voltage** (V) - Voltage per phase (if available)
- **Energy Import** (kWh) - Total consumed energy
- **Energy Export** (kWh) - Total produced energy
- **Battery SOC** (%) - Battery state of charge
- **Battery Power** (W) - Battery charge/discharge power
- **WiFi Signal** (dBm) - Signal strength
- **Uptime** (s) - Device uptime
- **Free Heap** (bytes) - Available memory
- **P1 Data Valid** - Binary sensor showing connection status

### Controls (From Home Assistant)
- **Operation Mode** - Select dropdown with options:
  - Unmodified Forward
  - Off (Prevent Charging/Discharging)
  - Force Charge
  - Force Discharge
  - Charge Only
  - Discharge Only

- **Battery Phase** - Select L1, L2, or L3
- **Modify Phase** - Select L1, L2, or L3
- **Force Power** - Number slider (0-20000 W)

## Home Assistant Device
After connection, you'll see a new device called **"ControlZinvolt P1"** in Home Assistant under:
**Settings → Devices & Services → MQTT**

Click on the device to see all entities and configure them in your dashboards.

## MQTT Topics Structure
```
controlzinvoltp1/<device_id>/state         - All sensor states (JSON)
controlzinvoltp1/<device_id>/status        - Online/offline status
controlzinvoltp1/<device_id>/cmd/*         - Command topics for controls
homeassistant/*/controlzinvoltp1_*/*/config - Discovery configurations
```

## Troubleshooting

### Device Not Appearing in Home Assistant
1. Check MQTT broker is running
2. Verify MQTT integration is configured in HA
3. Check device serial monitor for MQTT connection messages
4. Verify IP address and credentials are correct
5. Check firewall settings

### Connection Issues
- Serial monitor will show: `Connecting to MQTT broker... Connected!`
- If failed: `Failed, rc=<error_code>`
  - rc=-2: Network failed
  - rc=-4: Connection timeout
  - rc=5: Connection refused (wrong credentials)

### Re-discover Devices
If you need to force re-discovery:
1. Restart the ESP32 device
2. Discovery messages are sent every 5 minutes automatically
3. Or restart Home Assistant MQTT integration

## Creating Automations

Example automation to force charge when solar production is high:

```yaml
automation:
  - alias: "Force Charge on High Solar"
    trigger:
      - platform: numeric_state
        entity_id: sensor.solar_power
        above: 3000
    action:
      - service: select.select_option
        target:
          entity_id: select.controlzinvolt_p1_operation_mode
        data:
          option: "Force Charge"
```

## Energy Dashboard Integration

Add the P1 sensors to your Home Assistant Energy Dashboard:
1. Go to **Settings → Dashboards → Energy**
2. Click "Add Consumption"
3. Select `sensor.controlzinvolt_p1_energy_import`
4. Click "Add Production"
5. Select `sensor.controlzinvolt_p1_energy_export`

## Battery Data Integration

The system exposes `battery_soc` and `battery_power` sensors. If you want to import battery data from your existing HA sensors, you can create a simple automation or use the REST API to update these values.

## Data Update Frequency
- Sensor data publishes every **2 seconds**
- Discovery messages refresh every **5 minutes**
- Commands are processed immediately

## Security Note
For production use, always enable MQTT authentication and consider using TLS/SSL for MQTT communication.
