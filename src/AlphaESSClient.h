/*
 * AlphaESSClient.h - AlphaESS API client for EVA battery data
 */

#ifndef ALPHAESSCLIENT_H
#define ALPHAESSCLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "BatteryApiClient.h"

class AlphaESSClient : public BatteryApiClient {
public:
  AlphaESSClient(Config* config);
  
  void begin() override;
  void loop() override;
  bool fetchBatteryData(const String& p1Timestamp);
  void requestFetch(const String& p1Timestamp) override;  // Non-blocking request
  String getBackendName() const override { return "alphaess"; }
  
  bool isDataValid() const { return data_valid_; }
  float getSOC() const { return soc_; }
  float getBatteryPower() const { return battery_power_; }
  float getGridPower() const { return grid_power_; }
  
private:
  Config* config_;
  WiFiClientSecure client_;
  
  bool data_valid_;
  float soc_;
  float battery_power_;
  float grid_power_;
  unsigned long last_fetch_;
  
  // Non-blocking request state
  bool fetch_requested_;
  String requested_timestamp_;
  
  String calculateSign(const String& timestamp);
  unsigned long parseP1Timestamp(const String& p1Timestamp);
  bool makeAPIRequest(const String& endpoint, const String& timestamp, JsonDocument& doc);
};

#endif // ALPHAESSCLIENT_H
