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

class AlphaESSClient {
public:
  AlphaESSClient(Config* config);
  
  void begin();
  void loop();
  bool fetchBatteryData(const String& p1Timestamp);
  
  bool isDataValid() const { return _dataValid; }
  float getSOC() const { return _soc; }
  float getBatteryPower() const { return _batteryPower; }
  float getGridPower() const { return _gridPower; }
  
private:
  Config* _config;
  WiFiClientSecure _client;
  
  bool _dataValid;
  float _soc;
  float _batteryPower;
  float _gridPower;
  unsigned long _lastFetch;
  
  String calculateSign(const String& timestamp);
  unsigned long parseP1Timestamp(const String& p1Timestamp);
  bool makeAPIRequest(const String& endpoint, const String& timestamp, JsonDocument& doc);
};

#endif // ALPHAESSCLIENT_H
