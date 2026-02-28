/*
 * ZinvoltClient.h - Zinvolt API client for battery data
 */

#ifndef ZINVOLTCLIENT_H
#define ZINVOLTCLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "BatteryApiClient.h"

class ZinvoltClient : public BatteryApiClient {
public:
  ZinvoltClient(Config* config);

  void begin() override;
  void loop() override;
  void requestFetch(const String& p1Timestamp) override;
  String getBackendName() const override { return "zinvolt"; }

private:
  Config* config_;
  bool data_valid_;
  float soc_;
  float battery_power_;
  float grid_power_;
  unsigned long last_fetch_;

  bool fetch_requested_;
  String requested_timestamp_;

  String zinvolt_token_;
  String zinvolt_battery_id_;
  unsigned long zinvolt_token_fetched_ms_;

  bool fetchBatteryData();
  bool zinvoltLogin();
  bool resolveZinvoltBatteryId();
  bool fetchOnlineStatus(bool& online);
  bool fetchPhotovoltaicData(float& solarPower);
  bool zinvoltRequest(const String& method, const String& url, const String& body, JsonDocument& doc, bool includeAuth = true);
};

#endif // ZINVOLTCLIENT_H
