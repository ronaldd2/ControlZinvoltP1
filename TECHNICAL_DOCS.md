# ControlZinvoltP1 - Technical Documentation

## System Architecture

### Overview
```
Smart Meter (P1 Out) → ESP32-S3 (Modify) → P1 Dongle → Zinvolt VT1000
                              ↓
                         Web Interface
                         REST API
                         TCP Stream
```

## Data Flow

1. **Smart Meter** sends P1 telegram every 10 seconds
2. **ESP32-S3** receives data on GPIO44 (RX)
3. **P1Parser** extracts relevant OBIS codes
4. **P1Modifier** modifies power values based on operation mode
5. **ESP32-S3** sends modified telegram on GPIO43 (TX)
6. **Zinvolt VT1000** receives modified data and adjusts battery behavior

## DSMR P1 Protocol

### Important OBIS Codes

| OBIS Code | Description | Unit |
|-----------|-------------|------|
| 1-0:1.7.0 | Total active power delivered | kW |
| 1-0:2.7.0 | Total active power received | kW |
| 1-0:21.7.0 | Active power L1 delivered | kW |
| 1-0:22.7.0 | Active power L1 received | kW |
| 1-0:41.7.0 | Active power L2 delivered | kW |
| 1-0:42.7.0 | Active power L2 received | kW |
| 1-0:61.7.0 | Active power L3 delivered | kW |
| 1-0:62.7.0 | Active power L3 received | kW |
| 1-0:32.7.0 | Voltage L1 | V |
| 1-0:52.7.0 | Voltage L2 | V |
| 1-0:72.7.0 | Voltage L3 | V |
| 1-0:31.7.0 | Current L1 | A |
| 1-0:51.7.0 | Current L2 | A |
| 1-0:71.7.0 | Current L3 | A |

### Example P1 Telegram Structure
```
/ISK5\2M550E-1012

1-3:0.2.8(50)
0-0:1.0.0(210117180000W)
1-0:1.8.1(000123.456*kWh)
1-0:2.8.1(000789.012*kWh)
1-0:1.7.0(00.500*kW)
1-0:2.7.0(00.000*kW)
1-0:21.7.0(00.200*kW)
1-0:22.7.0(00.000*kW)
1-0:41.7.0(00.150*kW)
1-0:42.7.0(00.000*kW)
1-0:61.7.0(00.150*kW)
1-0:62.7.0(00.000*kW)
1-0:32.7.0(230.0*V)
1-0:52.7.0(229.5*V)
1-0:72.7.0(231.2*V)
!ABCD
```

## Operation Modes Explained

### 1. Unmodified Forward
- **Purpose:** Pass P1 data without changes
- **Use Case:** Normal operation, no battery control
- **Modification:** None

### 2. Off Mode
- **Purpose:** Stop battery charging/discharging
- **Use Case:** Emergency stop, maintenance
- **Modification:** Set power consumption/generation to near zero

### 3. Force Charge Mode
- **Purpose:** Force battery to charge at maximum rate
- **Use Case:** Charge battery during low electricity prices
- **Modification:** Increase reported power consumption by force power value
- **Example:** Real consumption 500W + Force 5000W = 5500W reported

### 4. Force Discharge Mode
- **Purpose:** Force battery to discharge at maximum rate
- **Use Case:** Discharge battery during high electricity prices
- **Modification:** Increase reported power generation by force power value
- **Example:** Real generation 200W + Force 5000W = 5200W reported

### 5. Charge Only (Future)
- **Purpose:** Only allow battery charging, prevent discharging
- **Requires:** Battery SOC information
- **Implementation:** Monitor battery state and block discharge when SOC is low

### 6. Discharge Only (Future)
- **Purpose:** Only allow battery discharging, prevent charging
- **Requires:** Battery SOC information
- **Implementation:** Monitor battery state and block charge when SOC is high

## Phase Configuration

### Battery Phase
Indicates which phase (L1, L2, or L3) the Zinvolt battery is physically connected to.

### Modify Phase
Indicates which phase in the P1 telegram should be modified to influence battery behavior.

**Note:** These should typically be the same phase, but can be different for advanced configurations.

## ESP32-S3 Implementation Details

### Multi-Core Task Distribution

**Core 0:**
- P1 Reader Task - Continuously reads from P1 serial port
- Parses incoming telegrams
- Broadcasts to TCP clients

**Core 1:**
- P1 Relay Task - Sends modified telegrams to output
- Web server handling
- REST API processing

### Memory Management

- **PSRAM:** Enabled for better performance with web interface
- **NVS:** Used for persistent configuration storage
- **Stack Size:** 4096 bytes per task (adjust if needed)

### Serial Configuration

```cpp
Serial1.begin(115200, SERIAL_8N1, P1_RX_PIN, P1_TX_PIN);
```

- **Baud Rate:** 115200 (DSMR 5.0 standard)
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1

## WiFi Configuration

### Initial Setup (WiFiManager)
1. Device creates AP: `ControlZinvoltP1-Setup`
2. Connect to AP with phone/computer
3. Captive portal opens automatically
4. Select your WiFi network and enter password
5. Device saves credentials and reboots

### Reconfiguration
To reset WiFi settings:
1. Hold BOOT button on ESP32-S3 during startup
2. Or modify code to call `wifiManager.resetSettings()`

## Security Considerations

⚠️ **Important Security Notes:**

1. **No Authentication:** Current implementation has no authentication on web interface or REST API
2. **Network Security:** Ensure device is on a secure, trusted network
3. **Future Enhancement:** Consider adding authentication for production use

## Troubleshooting

### No P1 Data Received
- Check GPIO pin connections (RX=44, TX=43)
- Verify baud rate matches smart meter (115200 for DSMR 5.0)
- Check serial monitor for parsing errors

### WiFi Connection Issues
- Reset WiFi credentials using WiFiManager
- Check WiFi signal strength (RSSI in web interface)
- Verify router supports 2.4 GHz (ESP32 doesn't support 5 GHz)

### Web Interface Not Loading
- Verify IP address (check serial monitor or router)
- Ensure device is on same network as your computer
- Check firewall settings

### Modification Not Working
- Verify operation mode is not "Unmodified"
- Check phase configuration matches your setup
- Monitor serial output for debugging info

### TCP Connection Issues
- Verify port 8888 is not blocked by firewall
- Use telnet to test: `telnet [DEVICE_IP] 8888`
- Check if P1 data is being received (web interface)

## Performance Metrics

- **P1 Telegram Processing:** ~10ms per telegram
- **Web Interface Refresh:** 2 seconds
- **TCP Broadcast:** Real-time (immediate)
- **Memory Usage:** ~150KB heap used (varies)
- **CPU Usage:** <5% average

## Advanced Configuration

### Custom Pin Assignment

Edit [src/main.cpp](src/main.cpp):
```cpp
#define P1_RX_PIN 44  // Change to your RX pin
#define P1_TX_PIN 43  // Change to your TX pin
```

### Modify Update Interval

Edit [src/WebInterface.cpp](src/WebInterface.cpp):
```javascript
// Change 2000 to desired interval in milliseconds
setInterval(fetchData, 2000);
```

### Custom TCP Port

Edit [src/main.cpp](src/main.cpp):
```cpp
#define P1_TCP_PORT 8888  // Change to desired port
```

## API Integration Examples

### Python Example
```python
import requests

# Set force charge mode with 5000W
base_url = "http://192.168.1.100"

# Set mode
response = requests.get(f"{base_url}/api/mode?value=2")
print(response.json())

# Set power
response = requests.get(f"{base_url}/api/power?value=5000")
print(response.json())

# Get current data
response = requests.get(f"{base_url}/api/p1data")
data = response.json()
print(f"Total Power: {data['power']['total']} kW")
```

### Node.js Example
```javascript
const axios = require('axios');

const baseURL = 'http://192.168.1.100';

async function setForceCharge(watts) {
  await axios.get(`${baseURL}/api/mode?value=2`);
  await axios.get(`${baseURL}/api/power?value=${watts}`);
  console.log(`Force charge set to ${watts}W`);
}

async function getData() {
  const response = await axios.get(`${baseURL}/api/p1data`);
  console.log(response.data);
}

setForceCharge(5000);
getData();
```

### Home Assistant Integration
```yaml
# configuration.yaml
sensor:
  - platform: rest
    name: "Smart Meter Total Power"
    resource: "http://192.168.1.100/api/p1data"
    value_template: "{{ value_json.power.total }}"
    unit_of_measurement: "kW"
    scan_interval: 10

switch:
  - platform: rest
    name: "Battery Force Charge"
    resource: "http://192.168.1.100/api/mode"
    body_on: "value=2"
    body_off: "value=0"
```

## Testing Checklist

Before deploying to production:

- [ ] Verify P1 data is received correctly
- [ ] Test all operation modes
- [ ] Verify phase configuration
- [ ] Test web interface on multiple devices
- [ ] Test REST API endpoints
- [ ] Monitor for at least 24 hours
- [ ] Check for memory leaks
- [ ] Verify modified telegrams are valid
- [ ] Test WiFi reconnection after power loss
- [ ] Document your specific configuration

## Support and Resources

- **PlatformIO Documentation:** https://docs.platformio.org/
- **ESP32-S3 Datasheet:** https://www.espressif.com/en/products/socs/esp32-s3
- **DSMR Protocol:** https://www.netbeheernederland.nl/
- **ArduinoJson:** https://arduinojson.org/

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-17
