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
  float getActivePowerL1() const { return _activePowerL1; }
  float getActivePowerL2() const { return _activePowerL2; }
  float getActivePowerL3() const { return _activePowerL3; }
  float getActivePower(uint8_t phase) const {
    switch (phase) {
      case 0: return getTotalActivePower();
      case 1: return _activePowerL1;
      case 2: return _activePowerL2;
      case 3: return _activePowerL3;
      default: return 0;
    }
  }
  float getTotalActivePower() const { return _activePowerL1 + _activePowerL2 + _activePowerL3; }
  
  float getActivePowerDeliveredL1() const { return _activePowerDeliveredL1; }
  float getActivePowerDeliveredL2() const { return _activePowerDeliveredL2; }
  float getActivePowerDeliveredL3() const { return _activePowerDeliveredL3; }
  float getActivePowerDelivered(uint8_t phase) const {
    switch (phase) {
      case 0: return (_activePowerDeliveredL1 + _activePowerDeliveredL2 + _activePowerDeliveredL3);
      case 1: return _activePowerDeliveredL1;
      case 2: return _activePowerDeliveredL2;
      case 3: return _activePowerDeliveredL3;
      default: return 0;
    }
  }
  
  float getTotalEnergyImport() const { return _totalEnergyImport; }
  float getTotalEnergyExport() const { return _totalEnergyExport; }
  
  float getCurrentL1() const { return _currentL1; }
  float getCurrentL2() const { return _currentL2; }
  float getCurrentL3() const { return _currentL3; }
  float getCurrent(uint8_t phase) const {
    switch (phase) {
      case 0: return (_currentL1 + _currentL2 + _currentL3);
      case 1: return _currentL1;
      case 2: return _currentL2;
      case 3: return _currentL3;
      default: return 0;
    }
  }
  
  float getVoltageL1() const { return _voltageL1; }
  float getVoltageL2() const { return _voltageL2; }
  float getVoltageL3() const { return _voltageL3; }
  float getVoltage(uint8_t phase) const {
    switch (phase) {
      case 1: return _voltageL1;
      case 2: return _voltageL2;
      case 3: return _voltageL3;
      default: return 0;
    }
  }
  
  String getTimestamp() const { return _timestamp; }
  bool isValid() const { return _valid; }
  void setValid(bool ok) { _valid = ok; }
  
  // Get raw telegram
  String getRawTelegram() const { return _rawTelegram; }
  
  // CRC validation and calculation
  static String calculateCRC16(const String& data);
  static bool validateCRC(const String& telegram);
  
private:
  // Parsed data fields
  float _activePowerL1;
  float _activePowerL2;
  float _activePowerL3;
  
  float _activePowerDeliveredL1;
  float _activePowerDeliveredL2;
  float _activePowerDeliveredL3;
  
  float _totalEnergyImport;
  float _totalEnergyExport;
  
  float _currentL1;
  float _currentL2;
  float _currentL3;
  
  float _voltageL1;
  float _voltageL2;
  float _voltageL3;
  
  String _timestamp;
  String _rawTelegram;
  bool _valid;
  
  // Helper functions
  float extractValue(const String& line);
  String extractObisValue(const String& telegram, const String& obisCode);
};

#endif // P1PARSER_H
