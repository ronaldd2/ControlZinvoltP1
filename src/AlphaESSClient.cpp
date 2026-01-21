/*
 * AlphaESSClient.cpp - Implementation of AlphaESS API client
 */

#include "AlphaESSClient.h"
#include <mbedtls/sha512.h>

// External logging functions from main.cpp
extern void logPrint(const String& msg);
extern void logPrintln(const String& msg);

AlphaESSClient::AlphaESSClient(Config* config) {
  _config = config;
  _dataValid = false;
  _soc = 0.0;
  _batteryPower = 0.0;
  _gridPower = 0.0;
  _lastFetch = 0;
}

void AlphaESSClient::begin() {
  _client.setInsecure();  // Skip certificate validation (or load CA cert if needed)
  logPrintln("[EVA] AlphaESS client initialized");
}

void AlphaESSClient::loop() {
  // Intentionally empty - fetching is triggered by P1 telegram reception
}

bool AlphaESSClient::fetchBatteryData(const String& p1Timestamp) {
  if (!_config->evaEnabled) {
    return false;
  }
  
  unsigned long now = millis();
  if (now - _lastFetch < 10000) {
    return false;  // Throttle to once per 10 seconds
  }
  _lastFetch = now;
  
  if (_config->evaSerialNumber.isEmpty() || _config->evaAppId.isEmpty() || _config->evaAppSecret.isEmpty()) {
    logPrintln("[EVA] Missing configuration");
    _dataValid = false;
    return false;
  }
  
  // Convert P1 timestamp to epoch time
  unsigned long epochTime = parseP1Timestamp(p1Timestamp);
  if (epochTime == 0) {
    epochTime = now / 1000;  // Fallback to system uptime
  }
  
  String timestamp = String(epochTime);
  
  JsonDocument doc;
  String endpoint = "/api/getLastPowerData?sysSn=" + _config->evaSerialNumber;
  
  if (makeAPIRequest(endpoint, timestamp, doc)) {
    if (doc["code"] == 200 && doc["data"].is<JsonObject>()) {
      JsonObject data = doc["data"];
      
      _soc = data["soc"] | 0.0f;
      float pbat = data["pbat"] | 0.0f;
      float prealL1 = data["prealL1"] | 0.0f;
      
      // pbat: negative = charging, positive = discharging
      _batteryPower = pbat;
      _gridPower = prealL1;
      
      // Update config with latest values
      _config->batterySOC = _soc;
      _config->batteryPower = _batteryPower;
      _config->gridPower = _gridPower;
      
      _dataValid = true;
      
      logPrint("[EVA] Battery SOC: ");
      logPrint(String(_soc, 1));
      logPrint("%, Power: ");
      logPrint(String(_batteryPower, 0));
      logPrint("W, Grid: ");
      logPrint(String(_gridPower, 0));
      logPrintln("W");
    } else {
      logPrint("[EVA] API error: ");
      logPrintln(doc["msg"] | "Unknown error");
      _dataValid = false;
    }
  } else {
    _dataValid = false;
    return false;
  }
  
  return _dataValid;
}

String AlphaESSClient::calculateSign(const String& timestamp) {
  String input = _config->evaAppId + _config->evaAppSecret + timestamp;
  
  unsigned char hash[64];
  mbedtls_sha512_context ctx;
  mbedtls_sha512_init(&ctx);
  mbedtls_sha512_starts(&ctx, 0);  // 0 = SHA-512 (not SHA-384)
  mbedtls_sha512_update(&ctx, (const unsigned char*)input.c_str(), input.length());
  mbedtls_sha512_finish(&ctx, hash);
  mbedtls_sha512_free(&ctx);
  
  String hashStr = "";
  for (int i = 0; i < 64; i++) {
    if (hash[i] < 16) hashStr += "0";
    hashStr += String(hash[i], HEX);
  }
  
  return hashStr;
}

unsigned long AlphaESSClient::parseP1Timestamp(const String& p1Timestamp) {
  // P1 timestamp format: YYMMDDhhmmssX (e.g., "230117180000W" for winter time)
  // X can be 'W' (winter/standard time) or 'S' (summer/daylight saving time)
  
  if (p1Timestamp.length() < 13) {
    return 0;
  }
  
  int year = 2000 + p1Timestamp.substring(0, 2).toInt();
  int month = p1Timestamp.substring(2, 4).toInt();
  int day = p1Timestamp.substring(4, 6).toInt();
  int hour = p1Timestamp.substring(6, 8).toInt();
  int minute = p1Timestamp.substring(8, 10).toInt();
  int second = p1Timestamp.substring(10, 12).toInt();
  char dst = p1Timestamp.charAt(12);
  
  // Convert to epoch time (UTC)
  // Simple conversion assuming CET timezone (UTC+1 in winter, UTC+2 in summer)
  struct tm timeinfo = {0};
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  timeinfo.tm_hour = hour;
  timeinfo.tm_min = minute;
  timeinfo.tm_sec = second;
  
  time_t epochTime = mktime(&timeinfo);
  
  // Adjust for timezone (CET/CEST)
  if (dst == 'S') {
    epochTime -= 2 * 3600;  // CEST is UTC+2
  } else {
    epochTime -= 1 * 3600;  // CET is UTC+1
  }
  
  return (unsigned long)epochTime;
}

bool AlphaESSClient::makeAPIRequest(const String& endpoint, const String& timestamp, JsonDocument& doc) {
  const char* host = "openapi.alphaess.com";
  const int httpsPort = 443;
  
  // Check WiFi connectivity
  if (WiFi.status() != WL_CONNECTED) {
    logPrintln("[EVA] WiFi not connected, cannot reach AlphaESS API");
    return false;
  }
  
  logPrint("[EVA] Connecting to ");
  logPrint(host);
  logPrintln("...");
  
  if (!_client.connect(host, httpsPort)) {
    logPrint("[EVA] Connection to AlphaESS API failed (WiFi: ");
    logPrint(WiFi.status() == WL_CONNECTED ? "OK" : "DOWN");
    logPrintln(")");
    return false;
  }
  
  logPrintln("[EVA] Connected, sending request...");
  String sign = calculateSign(timestamp);
  
  // Build HTTP request
  String request = "GET " + endpoint + " HTTP/1.1\r\n";
  request += "Host: " + String(host) + "\r\n";
  request += "appID: " + _config->evaAppId + "\r\n";
  request += "appSecret: " + _config->evaAppSecret + "\r\n";
  request += "timeStamp: " + timestamp + "\r\n";
  request += "sign: " + sign + "\r\n";
  request += "Connection: close\r\n\r\n";
  
  _client.print(request);
  
  // Wait for response
  unsigned long timeout = millis();
  while (_client.available() == 0) {
    if (millis() - timeout > 5000) {
      logPrintln("[EVA] API request timeout");
      _client.stop();
      return false;
    }
    delay(10);
  }
  
  logPrintln("[EVA] Received response, parsing...");
  
  // Skip HTTP headers
  bool headersEnded = false;
  while (_client.available()) {
    String line = _client.readStringUntil('\n');
    if (line == "\r") {
      headersEnded = true;
      break;
    }
  }
  
  if (!headersEnded) {
    logPrintln("[EVA] Failed to parse HTTP headers");
    _client.stop();
    return false;
  }
  
  // Read JSON body
  String body = _client.readString();
  _client.stop();
  
  logPrint("[EVA] JSON body length: ");
  logPrintln(String(body.length()));
  
  // Parse JSON
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    logPrint("[EVA] JSON parse error: ");
    logPrintln(error.c_str());
    logPrint("[EVA] Raw body: ");
    logPrintln(body);
    return false;
  }
  
  return true;
}
