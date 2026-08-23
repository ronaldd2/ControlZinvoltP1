# CRC Validation and Modified Telegram Streaming - Implementation Summary

## Changes Made

### 1. CRC16 Calculation and Validation (P1Parser)

Added CRC16 calculation function with polynomial 0xA001 (P1 standard):

**P1Parser.h:**
```cpp
// CRC validation and calculation
static String calculateCRC16(const String& data);
static bool validateCRC(const String& telegram);
```

**P1Parser.cpp:**
- `calculateCRC16(data)` - Calculates CRC16 for P1 telegrams, returns 4-char hex string
- `validateCRC(telegram)` - Validates received telegram CRC, extracts from after '!' character

Usage:
```cpp
// Validate received telegram
if (!P1Parser::validateCRC(buffer)) {
    logPrintln("CRC validation failed!");
}

// Calculate CRC for data
String crc = P1Parser::calculateCRC16(dataString);
```

### 2. CRC Recalculation in Modified Telegrams (P1Modifier)

**P1Modifier.h:**
- Renamed `calculateChecksum()` to `recalculateCRC()`

**P1Modifier.cpp:**
- `recalculateCRC(telegram)` - Strips old CRC and appends correct CRC to modified telegram
- Called automatically in `modify()` function before returning modified telegram
- Ensures modified telegrams have valid CRC for downstream devices

### 3. Dual TCP Server for Original and Modified Telegrams

**main.cpp - Global Variables:**
```cpp
#define P1_TCP_PORT 9988        // Original P1 data (existing)
#define P1_MODIFIED_TCP_PORT 9989  // Modified P1 data (new)

AsyncServer tcpServer(P1_TCP_PORT);            // Original
AsyncServer tcpModifiedServer(P1_MODIFIED_TCP_PORT);  // Modified

std::vector<AsyncClient*> tcpClients;           // Original clients
std::vector<AsyncClient*> tcpModifiedClients;   // Modified clients
```

**New Functions:**
- `setupTCPServer()` - Updated to start both servers
- `handleNewModifiedTCPClient()` - Handles connections to modified TCP server
- `broadcastModifiedP1Data()` - Broadcasts modified telegrams to clients

**Data Flow:**
```
Smart Meter P1 Input
    ↓
readP1Task (ESP32 reads data)
    ↓
P1Parser.validateCRC() (Verify received telegram)
    ↓
broadcastP1Data() → TCP Port 9988 (Original unmodified telegram)
    ↓
P1Modifier.modify() → P1Modifier.recalculateCRC()
    ↓
broadcastModifiedP1Data() → TCP Port 9989 (Modified telegram with correct CRC)
    ↓
relayP1Task (Send via P1 serial output)
```

## Usage

### Connect to Original P1 Telegrams:
```bash
# TCP connection on port 9988
telnet <ESP32_IP> 9988
```

### Connect to Modified P1 Telegrams:
```bash
# TCP connection on port 9989
telnet <ESP32_IP> 9989
```

### Monitor Logs (Telnet):
```bash
telnet <ESP32_IP> 23
```

Log entries show:
- `[READ] CRC validation passed` - Received telegram has valid CRC
- `[READ] WARNING: CRC validation failed!` - CRC mismatch (telegram may be corrupted)
- `[RELAY] Modified P1 telegram sent` - Modified telegram sent with new CRC

## CRC Calculation Details

The CRC16 implementation uses:
- **Polynomial:** 0xA001 (reversed CRC-CCITT)
- **Input:** All bytes from '/' to '!' (inclusive)
- **Output:** 4-character uppercase hex string
- **Format in telegram:** `/data...!\n<CRC>\n`

Example:
```
/ISK5\2ME382-1004
...
...
!ABCD
```

Where `ABCD` is the 4-char CRC result.

## Testing

1. Upload the modified code to ESP32-S3
2. Connect to modified TCP port: `telnet 192.168.12.57 9989`
3. Verify telegrams arrive with modified power values
4. Check telnet logs for CRC validation messages
5. Modify power values via web interface and verify changes in stream

## Notes

- CRC validation is logged but does not reject invalid telegrams (logging only)
- Modified telegrams always get correct CRC recalculated
- Both TCP servers run simultaneously without conflict
- Original and modified telegrams can be captured for comparison
