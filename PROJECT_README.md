# ControlZinvoltP1 - ESP32-S3 P1 Port Controller

Control the P1 dongle for the Zinvolt VT1000 battery system with an ESP32-S3 microcontroller.

## 🎯 Project Overview

This project uses the [Universal P1 Port Dongle](https://github.com/Leotro-Engineering/Universal-P1-Port-Dongle) hardware to modify DSMR smart meter P1 telegrams, enabling sophisticated battery control for the Zinvolt VT1000 system.

## ✨ Key Features

* **P1 Telegram Relay & Modification** - Intercept and modify smart meter data in real-time
* **TCP P1 Reader** - Stream real smart meter data over TCP (port 8888)
* **Web Interface** - Modern, responsive dashboard for monitoring and control
* **Multiple Operation Modes:**
  - Unmodified Forward - Pass through without changes
  - Off Mode - Prevent charging/discharging
  - Force Charge - Force battery charging (configurable power)
  - Force Discharge - Force battery discharging (configurable power)
  - Charge Only - Only allow charging (future)
  - Discharge Only - Only allow discharging (future)
* **Phase Control** - Select battery phase and which phase to modify
* **WiFi Configuration Portal** - Easy setup with WiFiManager
* **REST API** - Control via HTTP GET requests
* **Persistent Configuration** - Settings saved in ESP32 NVS

## 🔧 Hardware Requirements

* **ESP32-S3 Development Board** (ESP32-S3-DevKitC-1 or compatible)
* **Universal P1 Port Dongle** by Leotro Engineering
* **DSMR Smart Meter** with P1 port
* **USB Cable** for programming

### Pin Configuration

* **P1 RX:** GPIO44 (RX from smart meter)
* **P1 TX:** GPIO43 (TX to P1 output)
* **Serial Rate:** 115200 baud (DSMR standard)

## 📦 Software Setup

### Prerequisites

1. **Visual Studio Code** with **PlatformIO** extension installed
   - Download VS Code: https://code.visualstudio.com/
   - Install PlatformIO: https://platformio.org/install/ide?install=vscode

### Installation Steps

1. **Clone or download this repository**

2. **Open in VS Code**
   ```
   File > Open Folder > Select ControlZinvoltP1 folder
   ```

3. **PlatformIO will automatically download dependencies:**
   - ESPAsyncWebServer
   - AsyncTCP-esphome
   - ArduinoJson
   - WiFiManager

4. **Build the project**
   - Click the checkmark icon in the bottom toolbar
   - Or press `Ctrl+Alt+B`

5. **Upload to ESP32-S3**
   - Connect ESP32-S3 via USB
   - Click the arrow icon in the bottom toolbar
   - Or press `Ctrl+Alt+U`

## 🚀 First Time Setup

1. **Power on the ESP32-S3** after uploading firmware

2. **WiFi Configuration Portal will start automatically**
   - Look for WiFi network: `ControlZinvoltP1-Setup`
   - Connect with your phone/computer
   - Configure your WiFi credentials
   - Device will restart and connect to your network

3. **Find the device IP address**
   - Check your router's DHCP client list
   - Or use serial monitor: `Ctrl+Alt+S` in VS Code

4. **Access the web interface**
   - Open browser: `http://[DEVICE_IP]`
   - Example: `http://192.168.1.100`

## 🌐 Web Interface

The web interface provides:
- **Real-time smart meter data** - Power, voltage, current per phase
- **Operation mode selection** - Choose how the battery should behave
- **Phase configuration** - Set battery and modification phases
- **Force power settings** - Configure charge/discharge power levels
- **System information** - WiFi status, uptime, IP address

## 📡 REST API

All endpoints use HTTP GET requests:

### Get Current Status
```
GET /api/status
```
Returns system info, WiFi status, P1 data validity, and current mode.

### Get P1 Data
```
GET /api/p1data
```
Returns real-time smart meter data (power, voltage, current, energy).

### Get Configuration
```
GET /api/config
```
Returns current operation mode, phases, and force power settings.

### Set Operation Mode
```
GET /api/mode?value=[0-5]
```
- `0` = Unmodified
- `1` = Off
- `2` = Force Charge
- `3` = Force Discharge
- `4` = Charge Only (future)
- `5` = Discharge Only (future)

### Set Phase
```
GET /api/phase?battery=[1-3]&modify=[1-3]
```
- `battery` = Phase where battery is connected (1, 2, or 3)
- `modify` = Phase to modify in P1 telegram (1, 2, or 3)

### Set Force Power
```
GET /api/power?value=[watts]
```
Set power level in Watts for force charge/discharge modes (0-20000).

## 🔌 TCP P1 Data Stream

Connect to port **8888** to receive real-time P1 telegrams:

```bash
# Example with netcat
nc [DEVICE_IP] 8888
```

Or use any TCP client to stream unmodified smart meter data.

## 📝 Example API Usage

```bash
# Set to force charge mode with 5000W
curl "http://192.168.1.100/api/mode?value=2"
curl "http://192.168.1.100/api/power?value=5000"

# Set battery on phase L1, modify phase L2
curl "http://192.168.1.100/api/phase?battery=1&modify=2"

# Get current status
curl "http://192.168.1.100/api/status"
```

## 🛠️ Development

### Project Structure

```
ControlZinvoltP1/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp           # Main application
│   ├── P1Parser.h/cpp     # P1 telegram parser
│   ├── P1Modifier.h/cpp   # P1 telegram modifier
│   ├── WebInterface.h/cpp # Web server & REST API
│   └── Config.h/cpp       # Configuration management
├── README.md
└── LICENSE
```

### Serial Monitor

View debug output and system logs:
- In VS Code: Click serial monitor icon or press `Ctrl+Alt+S`
- Baud rate: 115200

### Modifying Pin Configuration

Edit pin definitions in [src/main.cpp](src/main.cpp):
```cpp
#define P1_RX_PIN 44  // GPIO44 (RX)
#define P1_TX_PIN 43  // GPIO43 (TX)
```

## ⚠️ Important Notes

* **P1 Output Timing:** The P1 output only sends data after receiving input from the smart meter
* **Zinvolt Compatibility:** The system works best with smart meters that output every 10 seconds (not every 1 second)
* **Safety:** Always test modifications carefully before connecting to production battery systems
* **Power Limits:** Ensure force power values are within your battery system's capabilities

## 🔮 Future Enhancements

* Battery SOC integration via ESS API
* Additional energy meter support for enhanced control
* Charge/Discharge Only modes with battery state awareness
* Historical data logging
* MQTT integration
* Home Assistant integration

## 📄 License

See [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## 🔗 Related Projects

* [Universal P1 Port Dongle](https://github.com/Leotro-Engineering/Universal-P1-Port-Dongle)
* [DSMR P1 Protocol Documentation](https://www.netbeheernederland.nl/dossiers/slimme-meter-15)

## 📧 Support

For issues and questions, please use the GitHub issue tracker.

---

**Made with ⚡ for the ESP32-S3 and Zinvolt VT1000**
