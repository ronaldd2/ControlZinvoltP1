/*
 * P1Parser.h - Parser for DSMR P1 Smart Meter Telegrams
 * 
 * Parses P1 telegrams and extracts relevant data fields
 */

#ifndef P1PARSER_H
#define P1PARSER_H

#include <Arduino.h>
#include <map>

class P1Parser {
public:
  P1Parser();
  
  // Parse a complete P1 telegram
  bool parse(const String& telegram);
  
  // Get parsed values
  float getActivePowerL1() const { return activePowerL1; }
  float getActivePowerL2() const { return activePowerL2; }
  float getActivePowerL3() const { return activePowerL3; }
  float getTotalActivePower() const { return activePowerL1 + activePowerL2 + activePowerL3; }
  
  float getActivePowerDeliveredL1() const { return activePowerDeliveredL1; }
  float getActivePowerDeliveredL2() const { return activePowerDeliveredL2; }
  float getActivePowerDeliveredL3() const { return activePowerDeliveredL3; }
  
  float getTotalEnergyImport() const { return totalEnergyImport; }
  float getTotalEnergyExport() const { return totalEnergyExport; }
  
  float getCurrentL1() const { return currentL1; }
  float getCurrentL2() const { return currentL2; }
  float getCurrentL3() const { return currentL3; }
  
  float getVoltageL1() const { return voltageL1; }
  float getVoltageL2() const { return voltageL2; }
  float getVoltageL3() const { return voltageL3; }
  
  String getTimestamp() const { return timestamp; }
  bool isValid() const { return valid; }
  
  // Get raw telegram
  String getRawTelegram() const { return rawTelegram; }
  
private:
  // Parsed data fields
  float activePowerL1;
  float activePowerL2;
  float activePowerL3;
  
  float activePowerDeliveredL1;
  float activePowerDeliveredL2;
  float activePowerDeliveredL3;
  
  float totalEnergyImport;
  float totalEnergyExport;
  
  float currentL1;
  float currentL2;
  float currentL3;
  
  float voltageL1;
  float voltageL2;
  float voltageL3;
  
  String timestamp;
  String rawTelegram;
  bool valid;
  
  // Helper functions
  float extractValue(const String& line);
  String extractObisValue(const String& telegram, const String& obisCode);
};

#endif // P1PARSER_H
