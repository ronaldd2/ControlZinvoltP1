/*
 * P1Tasks.cpp - P1 telegram reading and relay task implementations
 */

#include "P1Tasks.h"
#include "BatteryApiSelector.h"
#include "HomeAssistant.h"
#include <Preferences.h>
#include <freertos/semphr.h>

// Hardware Serial for P1 Port (ESP32-S3)
#define P1_SERIAL Serial1
#define LED_PIN 9
#define TX_REQ_PIN 41

// External references from main.cpp
extern Preferences preferences;
extern BatteryApiSelector batteryApi;
extern HomeAssistant homeAssistant;

void acceptTCPClients();
void broadcastP1Data(const String& telegram);

static SemaphoreHandle_t telegramMutex = xSemaphoreCreateMutex();

static String applyShellyMeasurements(const String& telegram) {
  String updated = P1Parser::updateVoltageValues(telegram, config.shellyVoltageL1,
                                                 config.shellyVoltageL2, config.shellyVoltageL3);
  auto replaceValue = [&updated](const char* obisCode, float value, unsigned int decimals) {
    int startIndex = updated.indexOf(obisCode);
    if (startIndex < 0) return;
    int openParen = updated.indexOf('(', startIndex);
    int closeParen = updated.indexOf(')', openParen);
    if (openParen < 0 || closeParen < 0) return;
    int unitIndex = updated.indexOf('*', openParen);
    String unit = (unitIndex >= 0 && unitIndex < closeParen) ? updated.substring(unitIndex, closeParen) : "";
    updated = updated.substring(0, openParen + 1) + String(value, decimals) + unit + updated.substring(closeParen);
  };
  auto replacePower = [&replaceValue](const char* importCode, const char* exportCode, float watts) {
    replaceValue(importCode, watts > 0 ? watts / 1000.0f : 0.0f, 3);
    replaceValue(exportCode, watts < 0 ? -watts / 1000.0f : 0.0f, 3);
  };
  float totalImportW = max(0.0f, config.shellyPowerL1) +
                       max(0.0f, config.shellyPowerL2) +
                       max(0.0f, config.shellyPowerL3);
  float totalExportW = -min(0.0f, config.shellyPowerL1) -
                       min(0.0f, config.shellyPowerL2) -
                       min(0.0f, config.shellyPowerL3);
  replaceValue("1-0:1.7.0", totalImportW / 1000.0f, 3);
  replaceValue("1-0:2.7.0", totalExportW / 1000.0f, 3);
  replacePower("1-0:21.7.0", "1-0:22.7.0", config.shellyPowerL1);
  replacePower("1-0:41.7.0", "1-0:42.7.0", config.shellyPowerL2);
  replacePower("1-0:61.7.0", "1-0:62.7.0", config.shellyPowerL3);
  replaceValue("1-0:31.7.0", config.shellyCurrentL1, 2);
  replaceValue("1-0:51.7.0", config.shellyCurrentL2, 2);
  replaceValue("1-0:71.7.0", config.shellyCurrentL3, 2);
  int crcPosition = updated.lastIndexOf('!');
  if (crcPosition >= 0) {
    String crcData = updated.substring(0, crcPosition + 1);
    updated = crcData + P1Parser::calculateCRC16(crcData) + "\r\n";
  }
  return updated;
}

void readP1Task(void* parameter) {
  if (telegramMutex == nullptr) {
    logPrintln("[READ] Failed to allocate telegram mutex");
    vTaskDelete(nullptr);
    return;
  }
  String buffer = "";
  unsigned long lastDebugTime = 0;
  unsigned long lastDataTime = 0;
  unsigned long lastTcpBroadcastMs = 0;
  bool hasTcpBroadcast = false;
  bool pendingTcpTelegram = false;
  String tcpTelegram;
  int telegramCount = 0;
  bool inTelegram = false;
  int crcCharsRead = 0;
  
  logPrintln("[READ] Task started - synchronizing with P1 stream...");
  
  while (true) {
    acceptTCPClients();

    if (millis() - lastDebugTime > 5000) {
      int available = P1_SERIAL.available();
      logPrint("[READ] Status - Available bytes: ");
      logPrint(String(available));
      logPrint(", Buffer size: ");
      logPrint(String(buffer.length()));
      logPrint(", InTelegram: ");
      logPrint(String(inTelegram));
      logPrint(", Telegrams: ");
      logPrint(String(telegramCount));
      logPrint(", Last data: ");
      logPrint(String((millis() - lastDataTime) / 1000));
      logPrintln(" sec ago");
      lastDebugTime = millis();
    }
    
    while (P1_SERIAL.available()) {
      char c = P1_SERIAL.read();
      lastDataTime = millis();
      
      if (c == '/') {
        buffer = "/";
        inTelegram = true;
        crcCharsRead = 0;
        logPrintln("[READ] Telegram start '/' detected");
        continue;
      }
      
      if (inTelegram) {
        buffer += c;
        
        if (c == '!') {
          crcCharsRead = 1;
          continue;
        }
        
        if (crcCharsRead) {  
          crcCharsRead++;
          
          if (crcCharsRead == 5+2) {
            logPrintln("[READ] Telegram end with CRC detected");
            telegramCount++;
            logPrint("[READ] Processing telegram #");
            logPrint(String(telegramCount));
            logPrint(" (");
            logPrint(String(buffer.length()));
            logPrintln(" bytes)");
            
            // Validate CRC first before any processing
            bool crcValid = false;
            try {
              crcValid = P1Parser::validateCRC(buffer);
            } catch (...) {
              logPrintln("[READ] CRC validation error");
              crcValid = false;
            }
            
            if (!crcValid) {
              logPrintln("[READ] WARNING: CRC validation failed! Skipping telegram.");
              buffer = "";
              inTelegram = false;
              crcCharsRead = 0;
              continue;
            }
            
            logPrintln("[READ] CRC validation passed!");
            
            // Parse telegram after CRC validation
            p1Parser.parse(buffer);
            p1Parser.setValid(true);

            String ts = p1Parser.getTimestamp(); // 260131130132W
            String dateKey = "";
            if (ts.length() >= 10 && ts.charAt(4) == '-' && ts.charAt(7) == '-') {
              dateKey = ts.substring(0, 10);
            } else if (ts.length() >= 6) {
              dateKey = ts.substring(0, 6);
            }

            if (dateKey.length() > 0 && dateKey != config.dayStartDate) {
              config.dayStartDate = dateKey;
              config.dayStartEnergyImport = p1Parser.getTotalEnergyImport();
              config.dayStartEnergyExport = p1Parser.getTotalEnergyExport();
              config.save(preferences);
              logPrint("Daily baseline set: ");
              logPrint(dateKey);
              logPrint(" (import=");
              logPrint(String(config.dayStartEnergyImport, 3));
              logPrint(" kWh, export=");
              logPrint(String(config.dayStartEnergyExport, 3));
              logPrintln(" kWh)");
            }
            
            tcpTelegram = buffer;
            pendingTcpTelegram = true;
            
            // Request battery API fetch (non-blocking, will be processed in main loop)
            if (config.evaEnabled) {
              batteryApi.requestFetch(ts);
            }
            
            homeAssistant.requestPublish();
            
            config.actualPowerL1 = p1Parser.getActivePowerL1();
            config.actualPowerL2 = p1Parser.getActivePowerL2();
            config.actualPowerL3 = p1Parser.getActivePowerL3();
            config.actualTotalPower = p1Parser.getTotalActivePower();
            
            xSemaphoreTake(telegramMutex, portMAX_DELAY);
            currentP1Telegram = buffer;
            telegramComplete = true;
            telegramSent = false;
            xSemaphoreGive(telegramMutex);
            
            logPrint("[READ] Telegram queued for relay: ");
            logPrint(String(buffer.length()));
            logPrintln(" bytes");
            
            logPrintln("[READ] P1 telegram received and processed");

            buffer = "";
            inTelegram = false;
            crcCharsRead = 0;
          }
        }
        
        if (buffer.length() > 2048) {
          logPrintln("[READ] WARNING: Buffer overflow, resetting");
          buffer = "";
          inTelegram = false;
          crcCharsRead = 0;
        }
      }
    }
    
    unsigned long now = millis();
    bool shellyAvailable = config.shellyDataValid && now - config.shellyLastUpdateMs < 30000;
    bool tcpOutputDue = shellyAvailable ? (!hasTcpBroadcast || now - lastTcpBroadcastMs >= 1000) :
                                         pendingTcpTelegram;
    if (!tcpTelegram.isEmpty() && tcpOutputDue) {
      String actualTelegram = shellyAvailable ? applyShellyMeasurements(tcpTelegram) : tcpTelegram;
      if (P1Parser::validateCRC(actualTelegram)) {
        broadcastP1Data(actualTelegram);
      } else {
        logPrintln("[READ] Invalid TCP output CRC; telegram not broadcast");
      }
      lastTcpBroadcastMs = millis();
      hasTcpBroadcast = true;
      pendingTcpTelegram = false;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void relayP1Task(void* parameter) {
  if (telegramMutex == nullptr) {
    logPrintln("[RELAY] Failed to allocate telegram mutex");
    vTaskDelete(nullptr);
    return;
  }
  unsigned long lastDebugTime = 0;
  unsigned long lastSendMs = 0;
  bool hasSent = false;
  bool pendingTelegram = false;
  String meterTelegram;
  
  logPrintln("[RELAY] Task started");
  
  while (true) {
    if (millis() - lastDebugTime > 5000) {
      logPrint("[RELAY] Status - telegramComplete: ");
      logPrint(String(telegramComplete));
      logPrint(", telegramSent: ");
      logPrint(String(telegramSent));
      logPrint(", TX_REQ: ");
      logPrintln(String(digitalRead(TX_REQ_PIN)));
      lastDebugTime = millis();
    }
    
    xSemaphoreTake(telegramMutex, portMAX_DELAY);
    if (telegramComplete) {
      meterTelegram = currentP1Telegram;
      pendingTelegram = true;
      telegramComplete = false;
    }
    xSemaphoreGive(telegramMutex);

    unsigned long now = millis();
    bool shellyAvailable = config.shellyDataValid && now - config.shellyLastUpdateMs < 30000;
    bool outputDue = shellyAvailable ? (!hasSent || now - lastSendMs >= 1000) : pendingTelegram;
    bool canSend = !config.useTxReq || digitalRead(TX_REQ_PIN) == HIGH;

    if (!meterTelegram.isEmpty() && outputDue && canSend) {
      String sourceTelegram = shellyAvailable ? applyShellyMeasurements(meterTelegram) : meterTelegram;
      P1Parser sourceParser;
      sourceParser.parse(sourceTelegram);
      String modifiedTelegram = p1Modifier.modify(sourceTelegram, sourceParser, config.gridPower);
      int crcPosition = modifiedTelegram.lastIndexOf('!');
      if (crcPosition >= 0) {
        String crcData = modifiedTelegram.substring(0, crcPosition + 1);
        modifiedTelegram = crcData + P1Parser::calculateCRC16(crcData) + "\r\n";
      }

      if (P1Parser::validateCRC(modifiedTelegram)) {
        digitalWrite(LED_PIN, HIGH);
        P1_SERIAL.print(modifiedTelegram);
        P1_SERIAL.flush();
        digitalWrite(LED_PIN, LOW);
        lastSendMs = millis();
        hasSent = true;
        pendingTelegram = false;
        xSemaphoreTake(telegramMutex, portMAX_DELAY);
        if (!telegramComplete) telegramSent = true;
        xSemaphoreGive(telegramMutex);

        P1Parser modifiedParser;
        modifiedParser.parse(modifiedTelegram);
        config.modifiedPowerL1 = modifiedParser.getActivePowerL1();
        config.modifiedPowerL2 = modifiedParser.getActivePowerL2();
        config.modifiedPowerL3 = modifiedParser.getActivePowerL3();
        config.totalModifiedPower = modifiedParser.getTotalActivePower();
        homeAssistant.requestPublish();
        logPrintln(shellyAvailable ? "[RELAY] Shelly-driven P1 telegram sent (1 second interval)" :
                                   "[RELAY] Meter-driven P1 telegram sent");
      } else {
        logPrintln("[RELAY] Invalid output CRC; telegram not sent");
      }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
