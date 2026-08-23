/*
 * ZinvoltClient.cpp - Implementation of Zinvolt API client
 * use of https://github.com/joostlek/python-zinvolt as reference for API endpoints and data structure
 */

#include "ZinvoltClient.h"

extern void logPrint(const String& msg);
extern void logPrintln(const String& msg);

ZinvoltClient::ZinvoltClient(Config* config) {
  config_ = config;
  data_valid_ = false;
  soc_ = 0.0f;
  battery_power_ = 0.0f;
  grid_power_ = 0.0f;
  last_fetch_ = 0;
  fetch_requested_ = false;
  requested_timestamp_ = "";
  zinvolt_token_ = "";
  zinvolt_battery_id_ = "";
  zinvolt_token_fetched_ms_ = 0;
}

void ZinvoltClient::begin() {
  logPrintln("[EVA] Zinvolt client initialized");
}

void ZinvoltClient::loop() {
  if (fetch_requested_) {
    fetch_requested_ = false;
    if (fetchBatteryData()) {
      logPrint("[EVA] Backend=zinvolt, SOC=");
      logPrint(String(soc_, 1));
      logPrint("%, BattPower=");
      logPrint(String(battery_power_, 0));
      logPrint("W, GridPower=");
      logPrint(String(grid_power_, 0));
      logPrint("W, CoC=");
      logPrint(String(config_->batteryApiCocPower, 3));
      logPrint("kW, MeterPower=");
      logPrint(String(config_->batteryApiMeterPower, 0));
      logPrintln("W");
    }
  }
}

void ZinvoltClient::requestFetch(const String& p1Timestamp) {
  fetch_requested_ = true;
  requested_timestamp_ = p1Timestamp;
}

bool ZinvoltClient::fetchBatteryData() {
  if (!config_->evaEnabled) {
    return false;
  }

  unsigned long now = millis();
  if (now - last_fetch_ < (10000 - 200)) {
    return false;
  }
  last_fetch_ = now;

  if (config_->zinvoltEmail.isEmpty() || config_->zinvoltPassword.isEmpty()) {
    logPrintln("[EVA] Zinvolt missing configuration (email/password)");
    data_valid_ = false;
    return false;
  }

  if (zinvolt_token_.isEmpty() || (now - zinvolt_token_fetched_ms_ > 50UL * 60UL * 1000UL)) {
    if (!zinvoltLogin()) {
      data_valid_ = false;
      return false;
    }
  }

  if (!resolveZinvoltBatteryId()) {
    data_valid_ = false;
    return false;
  }

  JsonDocument stateDoc;
  String stateUrl = "https://app.zinvolt.com/api/public/v2/system/" + zinvolt_battery_id_ + "/basic/current-state";
  if (!zinvoltRequest("GET", stateUrl, "", stateDoc, true)) {
    data_valid_ = false;
    return false;
  }

  JsonObject currentPower = stateDoc["currentPower"];
  if (currentPower.isNull()) {
    logPrintln("[EVA] Zinvolt current-state missing currentPower");
    data_valid_ = false;
    return false;
  }

  soc_ = currentPower["soc"] | 0.0f;
  config_->batteryApiCocPower = currentPower["coc"] | 0.0f;
  battery_power_ = currentPower["pbt"] | 0.0f;
  grid_power_ = currentPower["pso"] | 0.0f;

  float meterPower = 0.0f;
  JsonArray meterPowerArray = currentPower["meterPower"].as<JsonArray>();
  if (!meterPowerArray.isNull()) {
    for (JsonVariant meterItem : meterPowerArray) {
      if (meterItem["power"].is<float>() || meterItem["power"].is<int>()) {
        meterPower += (float)meterItem["power"];
      }
    }
  }
  config_->batteryApiMeterPower = meterPower;

  config_->batterySOC = soc_;
  config_->batteryPower = battery_power_;
  config_->gridPower = grid_power_;
/*
  bool isOnline = true;
  if (fetchOnlineStatus(isOnline)) {
    if (!isOnline) {
      logPrintln("[EVA] Zinvolt reports battery offline");
    }
  }
  */
/*
  float apiSolarPower = 0.0f;
  if (fetchPhotovoltaicData(apiSolarPower)) {
    config_->batteryApiSolarPower = apiSolarPower;
  } else if (currentPower["ppv"].is<float>() || currentPower["ppv"].is<int>()) {
    config_->batteryApiSolarPower = currentPower["ppv"];
  }

  if (currentPower["ppv"].is<float>() || currentPower["ppv"].is<int>()) {
    logPrint("[EVA] Zinvolt ppv from current-state: ");
    logPrint(String((float)currentPower["ppv"], 1));
    logPrintln("W (kept separate from MQTT solar)");
  }
*/
  data_valid_ = true;
  return true;
}

bool ZinvoltClient::zinvoltLogin() {
  JsonDocument responseDoc;
  String body = "{\"email\":\"" + config_->zinvoltEmail + "\",\"password\":\"" + config_->zinvoltPassword + "\"}";

  if (!zinvoltRequest("POST", "https://app.zinvolt.com/api/public/v2/login", body, responseDoc, false)) {
    logPrintln("[EVA] Zinvolt login request failed");
    return false;
  }

  String token = responseDoc["token"] | "";
  if (token.isEmpty()) {
    logPrintln("[EVA] Zinvolt login failed: token missing");
    return false;
  }

  zinvolt_token_ = token;
  zinvolt_token_fetched_ms_ = millis();
  logPrintln("[EVA] Zinvolt login OK");
  return true;
}

bool ZinvoltClient::resolveZinvoltBatteryId() {
  if (!config_->zinvoltBatteryId.isEmpty()) {
    zinvolt_battery_id_ = config_->zinvoltBatteryId;
    return true;
  }

  if (!zinvolt_battery_id_.isEmpty()) {
    return true;
  }

  JsonDocument batteriesDoc;
  if (!zinvoltRequest("GET", "https://app.zinvolt.com/api/public/v2/system/batteries", "", batteriesDoc, true)) {
    logPrintln("[EVA] Zinvolt battery list request failed");
    return false;
  }

  JsonArray batteries = batteriesDoc["batteries"];
  if (batteries.isNull() || batteries.size() == 0) {
    logPrintln("[EVA] Zinvolt returned no batteries");
    return false;
  }

  zinvolt_battery_id_ = batteries[0]["id"] | "";
  if (zinvolt_battery_id_.isEmpty()) {
    logPrintln("[EVA] Zinvolt battery id missing in response");
    return false;
  }

  logPrint("[EVA] Zinvolt auto-selected battery id: ");
  logPrintln(zinvolt_battery_id_);
  return true;
}

bool ZinvoltClient::fetchOnlineStatus(bool& online) {
  JsonDocument onlineDoc;
  String url = "https://app.zinvolt.com/api/public/v2/system/" + zinvolt_battery_id_ + "/basic/online-status";
  if (!zinvoltRequest("GET", url, "", onlineDoc, true)) {
    return false;
  }

  String status = onlineDoc["onlineStatus"] | "";
  status.toUpperCase();
  online = (status == "ONLINE");
  return true;
}

bool ZinvoltClient::fetchPhotovoltaicData(float& solarPower) {
  solarPower = 0.0f;
  JsonDocument pvDoc;
  String url = "https://app.zinvolt.com/api/public/v2/system/" + zinvolt_battery_id_ + "/basic/pv-data";
  if (!zinvoltRequest("GET", url, "", pvDoc, true)) {
    return false;
  }

  JsonArray pvArray = pvDoc.as<JsonArray>();
  if (!pvArray.isNull()) {
    for (JsonVariant pvItem : pvArray) {
      if (pvItem["power"].is<float>() || pvItem["power"].is<int>()) {
        solarPower += (float)pvItem["power"];
      }
    }
    return true;
  }

  if (pvDoc["power"].is<float>() || pvDoc["power"].is<int>()) {
    solarPower = pvDoc["power"];
    return true;
  }

  return false;
}

bool ZinvoltClient::zinvoltRequest(const String& method, const String& url, const String& body, JsonDocument& doc, bool includeAuth) {
  if (WiFi.status() != WL_CONNECTED) {
    logPrintln("[EVA] WiFi not connected, cannot reach Zinvolt API");
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  HTTPClient http;
  if (!http.begin(secureClient, url)) {
    logPrintln("[EVA] Zinvolt HTTP begin failed");
    return false;
  }

  http.addHeader("User-Agent", "controlzinvoltp1/1");
  if (includeAuth && !zinvolt_token_.isEmpty()) {
    http.addHeader("Authorization", "Bearer " + zinvolt_token_);
  }

  int httpCode = -1;
  if (method == "POST") {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(body);
  } else {
    httpCode = http.GET();
  }

  String payload = http.getString();
  http.end();

  logPrint("[EVA][ZINVOLT][RAW] ");
  logPrint(url);
  logPrint(" -> ");
  logPrintln(payload);

  if (httpCode < 200 || httpCode >= 300) {
    logPrint("[EVA] Zinvolt HTTP error ");
    logPrint(String(httpCode));
    logPrint(": ");
    logPrintln(payload);
    if (includeAuth && httpCode == 401) {
      zinvolt_token_ = "";
    }
    return false;
  }

  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    logPrint("[EVA] Zinvolt JSON parse error: ");
    logPrintln(error.c_str());
    return false;
  }

  return true;
}
