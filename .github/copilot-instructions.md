# ControlZinvoltP1 - Copilot Instructions

## Project Overview
ESP32-S3 firmware that intercepts and modifies DSMR P1 smart meter telegrams to control Zinvolt VT1000 battery systems. Uses PlatformIO (Arduino framework) for an embedded environment with dual-core task management.

## Architecture

### Multi-Core Task Distribution (Critical)
- **Core 0**: P1 serial reading (`readP1Task`) - parses telegrams, validates CRC, broadcasts to TCP clients
- **Core 1**: P1 relay (`relayP1Task`) + Web server + MQTT handling
- Stack size: 4096 bytes per task (adjust if needed in [main.cpp](src/main.cpp))
- Communication: Global `currentP1Telegram` string with `telegramComplete` flag

### Data Flow
```
Smart Meter → GPIO1 RX → Parse → Validate CRC → TCP Port 9988 (original)
                                     ↓
                                  Modify → Recalculate CRC → TCP Port 9989 (modified)
                                     ↓
                                  GPIO42 TX → P1 Dongle → Zinvolt VT1000
```

### Key Components
- **P1Parser** ([P1Parser.cpp](src/P1Parser.cpp)): OBIS code extraction, CRC16 validation (polynomial 0xA001)
- **P1Modifier** ([P1Modifier.cpp](src/P1Modifier.cpp)): Telegram modification per operation mode, auto-recalculates CRC
- **WebInterface** ([WebInterface.cpp](src/WebInterface.cpp)): AsyncWebServer with HTTP Basic auth, embedded HTML/CSS/JS
- **Config** ([Config.cpp](src/Config.cpp)): NVS persistence for settings
- **HomeAssistant** ([HomeAssistant.cpp](src/HomeAssistant.cpp)): MQTT auto-discovery, publishes sensors/controls

## Critical Patterns

### P1 Telegram Handling
- Telegrams start with `/` and end with `!` + 4-char CRC
- Always validate CRC on receive: `P1Parser::validateCRC(telegram)`
- After modification, always recalculate: `P1Modifier::recalculateCRC(telegram)`
- OBIS codes use format `1-0:21.7.0` for phase-specific power values
- Power in kW (convert from W by dividing by 1000), voltage in V, current in A

### Operation Modes (OperationMode enum in [P1Modifier.h](src/P1Modifier.h))
- **UNMODIFIED**: Pass-through without changes
- **OFF**: Zero power to prevent charging/discharging (sets 0.001 kW to avoid div by zero)
- **FORCE_CHARGE**: Adds `forcePower` to delivered power on `modifyPhase`
- **FORCE_DISCHARGE**: Adds `forcePower` to received power on `modifyPhase`
- Modes CHARGE_ONLY/DISCHARGE_ONLY are placeholders (need battery SOC info)

### TCP Streaming
- Port **9988**: Original unmodified telegrams
- Port **9989**: Modified telegrams with recalculated CRC
- Use synchronous `WiFiServer` (not AsyncServer to avoid crashes during init)
- Client management in `acceptTCPClients()` - drops disconnected clients automatically

### Logging Pattern
- Use `logPrint()`/`logPrintln()` - outputs to both Serial (115200 baud) and Telnet (port 23)
- Add task prefixes: `[READ]`, `[RELAY]`, `[WEB]`, `[MQTT]`
- Example: `logPrintln("[READ] CRC validation passed");`

## Development Workflow

### Build & Upload
```bash
# Build (PlatformIO toolbar checkmark or)
pio run

# Upload USB (auto-detect COM port)
pio run --target upload

# Upload OTA (configured in platformio.ini)
pio run --target upload --upload-port 192.168.12.57

# Monitor serial
pio device monitor
```

**Note**: This is a Windows development machine. PlatformIO is NOT on the system PATH. Use the full path:
```powershell
& "C:\Users\micro\.platformio\penv\Scripts\platformio.exe" run
& "C:\Users\micro\.platformio\penv\Scripts\platformio.exe" run --target upload
```
Or use VS Code PlatformIO extension (Tasks > Build/Upload buttons)

### Pin Configuration (ESP32-S3)
- **GPIO1**: P1 RX (input from smart meter)
- **GPIO42**: P1 TX (output to P1 dongle)
- **GPIO2**: RX_REQ_PIN (constant high)
- **GPIO41**: TX_REQ_PIN (check before sending if `Config.useTxReq` enabled)
- **GPIO9**: LED_PIN (status indicator)

### Testing P1 Data
```bash
# Original telegrams
telnet <ESP32_IP> 9988

# Modified telegrams
telnet <ESP32_IP> 9989

# Logs
telnet <ESP32_IP> 23
```

## Project-Specific Conventions

### File Organization
- Headers include class definition + inline helpers
- Implementation files handle business logic only
- Global state in [main.cpp](src/main.cpp): `currentP1Telegram`, `telegramComplete`, `tcpClients`

### Configuration Management
- All settings persisted to NVS (ESP32 Non-Volatile Storage) in [Config.cpp](src/Config.cpp)
- Load on startup: `config.load(&preferences)`
- Save after changes: `config.save(&preferences)`
- Keys: "mode", "battery_phase", "modify_phase", "force_power", "mqtt_*", "web_*"

### Web Authentication
- HTTP Basic Auth on ALL endpoints (username/password from NVS)
- Pattern: `if (!request->authenticate(...)) return request->requestAuthentication();`
- Default credentials configured in [Config.cpp](src/Config.cpp)

### MQTT Integration
- Uses PubSubClient library
- Auto-discovery for Home Assistant (topics: `homeassistant/*/controlzinvoltp1_*`)
- State published to `controlzinvoltp1/<device_id>/state` as JSON
- Commands received on `controlzinvoltp1/<device_id>/cmd/*`
- Reconnect logic in [HomeAssistant.cpp](src/HomeAssistant.cpp): `loop()` checks connection

### JSON API Responses
- Use ArduinoJson v7.x: `JsonDocument doc;` (not v6 StaticJsonDocument)
- Pattern: Create doc → Fill data → Serialize → Send as `application/json`
- Example in [WebInterface.cpp](src/WebInterface.cpp): `handleGetStatus()`

## Dependencies & Libraries
- **ESPAsyncWebServer** + **AsyncTCP-esphome**: Async web server (handle with care, init crashes common)
- **ArduinoJson 7.x**: JSON parsing/serialization (breaking changes from v6)
- **WiFiManager 2.0.17**: Captive portal for WiFi config
- **PubSubClient**: MQTT client (max packet size 2048 in platformio.ini)

## Common Pitfalls
- **CRC validation**: Never send modified telegrams without recalculating CRC
- **Memory**: ESP32-S3-Mini-1 has 8MB flash, NO PSRAM - watch heap usage
- **FreeRTOS**: Don't use `delay()` in tasks - use `vTaskDelay(pdMS_TO_TICKS(ms))`
- **AsyncServer crashes**: Use synchronous `WiFiServer` for TCP streaming instead
- **Phase confusion**: `batteryPhase` (physical connection) vs `modifyPhase` (which OBIS codes to change)

## Documentation References
- Main docs: [PROJECT_README.md](PROJECT_README.md), [TECHNICAL_DOCS.md](TECHNICAL_DOCS.md)
- Quick setup: [QUICK_START.md](QUICK_START.md)
- CRC implementation: [CRC_AND_MODIFIED_TCP_CHANGES.md](CRC_AND_MODIFIED_TCP_CHANGES.md)
- MQTT setup: [HOMEASSISTANT_SETUP.md](HOMEASSISTANT_SETUP.md)
- File structure: [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md)
