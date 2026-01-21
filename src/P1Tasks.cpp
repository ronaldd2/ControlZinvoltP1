/*
 * P1Tasks.cpp - P1 telegram reading and relay task implementations
 */

#include "P1Tasks.h"
#include "AlphaESSClient.h"
#include "HomeAssistant.h"
#include <Preferences.h>

// Hardware Serial for P1 Port (ESP32-S3)
#define P1_SERIAL Serial1
#define LED_PIN 9
#define TX_REQ_PIN 41

// External references from main.cpp
extern Preferences preferences;
extern AlphaESSClient alphaESS;
extern HomeAssistant homeAssistant;

void acceptTCPClients();
void broadcastP1Data(const String& telegram);

void readP1Task(void* parameter) {
  String buffer = "";
  unsigned long lastDebugTime = 0;
  unsigned long lastDataTime = 0;
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
          
          if (crcCharsRead == 5) {
            logPrintln("[READ] Telegram end with CRC detected");
            telegramCount++;
            logPrint("[READ] Processing telegram #");
            logPrint(String(telegramCount));
            logPrint(" (");
            logPrint(String(buffer.length()));
            logPrintln(" bytes)");
            
            digitalWrite(LED_PIN, HIGH);
            p1Parser.parse(buffer);

            String ts = p1Parser.getTimestamp();
            String dateKey = "";
            if (ts.length() >= 10 && ts.charAt(4) == '-' && ts.charAt(7) == '-') {
              dateKey = ts.substring(0, 10);
            } else if (ts.length() >= 8) {
              dateKey = ts.substring(0, 8);
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
            
            broadcastP1Data(buffer);
            
            try {
              bool crcValid = P1Parser::validateCRC(buffer);
              p1Parser.setValid(crcValid);
              if (crcValid) {
                logPrintln("[READ] CRC validation passed!");
                
                if (config.evaEnabled && !config.evaSerialNumber.isEmpty()) {
                  String p1Timestamp = p1Parser.getTimestamp();
                  if (alphaESS.fetchBatteryData(p1Timestamp)) {
                    logPrint("[READ] AlphaESS: SOC=");
                    logPrint(String(alphaESS.getSOC(), 1));
                    logPrint("%, BattPower=");
                    logPrint(String(alphaESS.getBatteryPower()));
                    logPrint("W, GridPower=");
                    logPrint(String(alphaESS.getGridPower()));
                    logPrintln("W");
                  }
                }
                
                homeAssistant.requestPublish();
              } else {
                logPrintln("[READ] WARNING: CRC validation failed!");
              }
            } catch (...) {
              logPrintln("[READ] CRC validation error");
              p1Parser.setValid(false);
            }
            
            String modifiedTelegram = p1Modifier.modify(buffer, p1Parser, config.batteryPower);
            
            P1Parser modifiedParser;
            modifiedParser.parse(modifiedTelegram);
            config.modifiedPowerL1 = modifiedParser.getActivePowerL1();
            config.modifiedPowerL2 = modifiedParser.getActivePowerL2();
            config.modifiedPowerL3 = modifiedParser.getActivePowerL3();
            config.totalModifiedPower = modifiedParser.getTotalActivePower();

            config.actualPowerL1 = p1Parser.getActivePowerL1();
            config.actualPowerL2 = p1Parser.getActivePowerL2();
            config.actualPowerL3 = p1Parser.getActivePowerL3();
            config.actualTotalPower = p1Parser.getTotalActivePower();
            
            currentP1Telegram = modifiedTelegram;
            telegramComplete = true;
            telegramSent = false;
            
            logPrint("[READ] Telegram processed - Original: ");
            logPrint(String(buffer.length()));
            logPrint(" bytes, Modified: ");
            logPrint(String(modifiedTelegram.length()));
            logPrintln(" bytes");
            
            logPrintln("[READ] P1 telegram received and processed");
            delay(50);
            digitalWrite(LED_PIN, LOW);
            
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
    
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void relayP1Task(void* parameter) {
  unsigned long lastDebugTime = 0;
  
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
    
    if (telegramComplete && !telegramSent) {
      logPrintln("[RELAY] Processing telegram for transmission");
      
      bool canSend = !config.useTxReq || (digitalRead(TX_REQ_PIN) == HIGH);
      
      if (canSend) {
        logPrintln("[RELAY] Sending modified telegram...");
        P1_SERIAL.print(currentP1Telegram);
        
        telegramSent = true;
        telegramComplete = false;
        
        logPrintln("[RELAY] Modified P1 telegram sent");
      } else {
        logPrintln("[RELAY] Waiting for TX_REQ to go HIGH");
      }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
