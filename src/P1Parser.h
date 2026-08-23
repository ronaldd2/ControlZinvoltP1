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
  float getActivePowerL1() const { return active_power_l1_; }
  float getActivePowerL2() const { return active_power_l2_; }
  float getActivePowerL3() const { return active_power_l3_; }
  float getActivePower(uint8_t phase) const {
    switch (phase) {
      case 0: return getTotalActivePower();
      case 1: return active_power_l1_;
      case 2: return active_power_l2_;
      case 3: return active_power_l3_;
      default: return 0;
    }
  }
  float getTotalActivePower() const { return active_power_l1_ + active_power_l2_ + active_power_l3_; }
  
  float getActivePowerDeliveredL1() const { return active_power_delivered_l1_; }
  float getActivePowerDeliveredL2() const { return active_power_delivered_l2_; }
  float getActivePowerDeliveredL3() const { return active_power_delivered_l3_; }
  float getActivePowerDelivered(uint8_t phase) const {
    switch (phase) {
      case 0: return (active_power_delivered_l1_ + active_power_delivered_l2_ + active_power_delivered_l3_);
      case 1: return active_power_delivered_l1_;
      case 2: return active_power_delivered_l2_;
      case 3: return active_power_delivered_l3_;
      default: return 0;
    }
  }
  
  float getTotalEnergyImport() const { return total_energy_import_; }
  float getTotalEnergyExport() const { return total_energy_export_; }
  
  float getCurrentL1() const { return current_l1_; }
  float getCurrentL2() const { return current_l2_; }
  float getCurrentL3() const { return current_l3_; }
  float getCurrent(uint8_t phase) const {
    switch (phase) {
      case 0: return (current_l1_ + current_l2_ + current_l3_);
      case 1: return current_l1_;
      case 2: return current_l2_;
      case 3: return current_l3_;
      default: return 0;
    }
  }
  
  float getVoltageL1() const { return voltage_l1_; }
  float getVoltageL2() const { return voltage_l2_; }
  float getVoltageL3() const { return voltage_l3_; }
  float getVoltage(uint8_t phase) const {
    switch (phase) {
      case 1: return voltage_l1_;
      case 2: return voltage_l2_;
      case 3: return voltage_l3_;
      default: return 0;
    }
  }
  
  String getTimestamp() const { return timestamp_; }
  bool isValid() const { return valid_; }
  void setValid(bool ok) { valid_ = ok; }
  
  // Get DSMR version from telegram header (e.g., "/ISK5\2ME382-1004" -> "5")
  String getDsmrVersion() const;
  
  // Get raw telegram
  String getRawTelegram() const { return raw_telegram_; }
  
  // CRC validation and calculation
  static String calculateCRC16(const String& data);
  static bool validateCRC(const String& telegram);
  
private:
  // Parsed data fields
  float active_power_l1_;
  float active_power_l2_;
  float active_power_l3_;
  
  float active_power_delivered_l1_;
  float active_power_delivered_l2_;
  float active_power_delivered_l3_;
  
  float total_energy_import_;
  float total_energy_export_;
  
  float current_l1_;
  float current_l2_;
  float current_l3_;
  
  float voltage_l1_;
  float voltage_l2_;
  float voltage_l3_;
  
  String timestamp_;
  String raw_telegram_;
  String dsmr_version_;
  bool valid_;
  
  // Helper functions
  float extractValue(const String& line);
  String extractObisValue(const String& telegram, const String& obisCode);
};

#endif // P1PARSER_H
