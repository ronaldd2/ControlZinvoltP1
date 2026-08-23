# External Control Mode - Implementation Guide

## Overview

The **External Control Mode (Mode 6)** allows the P1 telegram modifications to be controlled remotely via REST API. This enables dynamic, real-time control of the Zinvolt battery system based on external conditions or commands.

## How It Works

### Normal Operation
When in External Control mode:

1. **API command received** → System immediately applies the power modification and sends the next P1 telegram with the modified values
2. **No API command for 60+ seconds** → System automatically switches to relay-through mode (original P1 data unmodified)
3. **New API command received** → System re-engages external control and applies new modifications

### Power Value Interpretation

- **Positive values (0 to 20000 W)**: Force charging
  - Shows high consumption (grid → house) in the modified P1 telegram
  - Battery sees demand and starts charging
  
- **Negative values (-20000 to 0 W)**: Force discharging
  - Shows high production (house → grid) in the modified P1 telegram
  - Battery sees surplus and starts discharging
  
- **Zero**: Effectively stops battery charging/discharging (passes original values)

## REST API Usage

### Set Mode to External Control
```bash
curl "http://192.168.1.100/api/mode?value=6"
```

Response:
```json
{
  "success": true,
  "mode": 6,
  "modeString": "External Control"
}
```

### Send External Control Power Command
```bash
# Force charging at 5000W
curl "http://192.168.1.100/api/external?value=5000"

# Force discharging at -3000W
curl "http://192.168.1.100/api/external?value=-3000"
```

Response:
```json
{
  "success": true,
  "externalPower": 5000.0,
  "mode": 6,
  "modeString": "External Control",
  "timeoutSeconds": 60,
  "info": "External control value received. Will auto-relay after 60 seconds if no new value received."
}
```

### Check Current Status
```bash
curl "http://192.168.1.100/api/status"
```

The response includes `modifier` section with:
- `externalPower`: Current external control power setting (if in mode 6)
- `mode`: Current operation mode (6 for External Control)
- `timeoutSeconds`: 60-second timeout value

## Implementation Details

### Code Changes

#### P1Modifier.h
- Added `MODE_EXTERNAL_CONTROL` enum value (mode 6)
- Added `externalControlPower` member variable (stores power in Watts)
- Added `externalControlLastUpdate` member variable (timestamp of last update)
- Added methods:
  - `setExternalControlPower(float watts)` - Set power and update timestamp
  - `getExternalControlPower()` - Retrieve current power setting
  - `getExternalControlLastUpdate()` - Get timestamp of last update
  - `isExternalControlValid()` - Check if control is still valid (< 60 sec)

#### P1Modifier.cpp
- Initialize external control variables in constructor
- Updated `getModeString()` to return "External Control" for mode 6
- Added logic in `modify()` function to handle MODE_EXTERNAL_CONTROL:
  - If valid (< 60 sec): Apply power modification (charging or discharging)
  - If expired (≥ 60 sec): Relay original P1 data unmodified

#### WebInterface.h & WebInterface.cpp
- Added `handleSetExternalControl()` method
- Added `/api/external` GET endpoint to receive power commands
- Updated mode validation from 0-5 to 0-6
- Input validation: power values constrained to ±20000W range

#### SettingsPage.cpp
- Added "External Control" option (value 6) to mode selection dropdown

#### PROJECT_README.md
- Updated API documentation with External Control mode
- Added `/api/external?value=[watts]` endpoint documentation
- Updated features list to include External Control mode

## Safety Features

1. **Power Limit Validation**: External control values are validated to be within ±20000W range
2. **Automatic Fallback**: 60-second timeout ensures safe operation if external control is interrupted
3. **Graceful Degradation**: Automatically relays original P1 data if control commands stop arriving
4. **Authentication**: All endpoints require HTTP Basic Auth (same as web interface)

## Usage Examples

### Scenario 1: Home Energy Management System
```bash
# Every 30 seconds, home management system sends current battery command
watch -n 30 'curl "http://esp32/api/external?value=CURRENT_SETPOINT"'
```

### Scenario 2: Scheduled Battery Control
```bash
# Force charge from 2am-6am (5000W)
# Off otherwise
0 2 * * * /usr/bin/curl "http://esp32/api/external?value=5000"
0 6 * * * /usr/bin/curl "http://esp32/api/external?value=0"
```

### Scenario 3: Dynamic Response to Grid Events
```bash
# Python script monitoring grid frequency
# Send discharge command if frequency drops
while True:
    freq = get_grid_frequency()
    if freq < 49.5:  # Grid stress
        requests.get("http://esp32/api/external?value=-5000")
    else:
        requests.get("http://esp32/api/external?value=0")
    time.sleep(5)
```

## Debugging

### Check Current Mode
```bash
curl "http://192.168.1.100/api/config" | jq '.mode'
```

### Check If External Control Is Active
```bash
curl "http://192.168.1.100/api/status" | jq '.modifier'
```

### View System Logs
```bash
telnet 192.168.1.100 23
# Check [WEB] entries for external control events
```

## Edge Cases

| Situation | Behavior |
|-----------|----------|
| Mode 6 + API command every 30s | Continuously applies control modifications |
| Mode 6 + No API command for 65s | Automatically relays original P1 data unmodified |
| Switch from Mode 6 to Mode 0 | Stops control, relays original data |
| API command with value 0 | Effective neutral, shows original power values |
| API command out of range (>20000) | Rejected with error message |
| Mode 6 + No phase configuration | Uses configured modify phase (same as other modes) |

## Performance Considerations

- External control updates have minimal latency (<100ms)
- 60-second timeout is sufficient for most control systems
- P1 telegrams are sent on standard 10-second intervals (smart meter dependent)
- Each API call updates the timestamp immediately
- No blocking operations in the timeout check

## Future Enhancements

- Configurable timeout value (currently hardcoded to 60 seconds)
- Power rate limiting (prevent rapid changes)
- Command history logging
- WebSocket support for real-time updates
- Integration with Home Assistant template switches
