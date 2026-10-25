#include "ShellyEm3.h"

#include <ArduinoJson.h>

extern void logPrintln(const String& msg);

ShellyEm3Reader::ShellyEm3Reader() : config_(nullptr) {
}

void ShellyEm3Reader::setConfig(Config* config) {
  config_ = config;
}

void ShellyEm3Reader::begin() {
  lastPacketMs_ = millis();
  lastReportMs_ = lastPacketMs_;
  if (!udp_.beginMulticast(IPAddress(224, 0, 0, 163), 3333)) {
    logPrintln("[SHELLY] Failed to join multicast group 224.0.0.163:3333");
  } else {
    logPrintln("[SHELLY] Listening for Shelly EM3 multicast updates on 224.0.0.163:3333");
  }
}

void ShellyEm3Reader::loop() {
  if (config_ == nullptr) {
    return;
  }

  int packetSize = udp_.parsePacket();
  if (packetSize <= 0) {
    unsigned long now = millis();
    if (now - lastPacketMs_ >= 30000 && now - lastReportMs_ >= 30000) {
      logPrintln("[SHELLY] No UDP packets received for 30 seconds on 224.0.0.163:3333; check multicast delivery to ESP32 WiFi");
      lastReportMs_ = now;
    }
    return;
  }

  lastPacketMs_ = millis();
  char buffer[2048];
  if (packetSize >= sizeof(buffer)) {
    logPrintln("[SHELLY] UDP packet too large: " + String(packetSize) + " bytes from " + udp_.remoteIP().toString());
    while (udp_.available()) {
      udp_.read(buffer, sizeof(buffer));
    }
    return;
  }
  int bytesRead = udp_.read(buffer, sizeof(buffer) - 1);
  if (bytesRead <= 0) {
    return;
  }

  buffer[bytesRead] = '\0';
  if (bytesRead >= 2 && buffer[0] == 'S' && buffer[1] == 'L') {
    if (bytesRead <= 12) {
      logPrintln("[SHELLY] Incomplete SL UDP header/payload");
      return;
    }
    parsePayload(String(buffer + 12));
  } else {
    parsePayload(String(buffer));
  }
}

void ShellyEm3Reader::parsePayload(const String& payload) {
  if (config_ == nullptr) {
    return;
  }

  String json = payload;
  if (json.startsWith("S")) {
    json.remove(0, 1);
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    logPrintln("[SHELLY] Failed to parse UDP JSON payload (" + String(payload.length()) + " bytes): " + err.c_str());
    return;
  }

  JsonObject em = doc["status"]["em:0"];
  if (em.isNull()) {
    em = doc["params"]["em:0"];
  }
  if (em.isNull()) {
    em = doc["result"]["em:0"];
  }
  if (em.isNull()) {
    unsigned long now = millis();
    if (now - lastReportMs_ >= 30000) {
      logPrintln("[SHELLY] UDP JSON received but no em:0 measurements; method=" + String(doc["method"] | "(none)"));
      lastReportMs_ = now;
    }
    return;
  }
  if (em["a_act_power"].isNull() && em["b_act_power"].isNull() &&
      em["c_act_power"].isNull() && em["total_act_power"].isNull() &&
      !config_->shellyDataValid) {
    return;
  }

  config_->shellyDataValid = true;
  config_->shellyPowerL1 = em["a_act_power"] | config_->shellyPowerL1;
  config_->shellyPowerL2 = em["b_act_power"] | config_->shellyPowerL2;
  config_->shellyPowerL3 = em["c_act_power"] | config_->shellyPowerL3;
  config_->shellyTotalPower = em["total_act_power"] | (config_->shellyPowerL1 + config_->shellyPowerL2 + config_->shellyPowerL3);
  config_->shellyVoltageL1 = em["a_voltage"] | config_->shellyVoltageL1;
  config_->shellyVoltageL2 = em["b_voltage"] | config_->shellyVoltageL2;
  config_->shellyVoltageL3 = em["c_voltage"] | config_->shellyVoltageL3;
  config_->shellyCurrentL1 = em["a_current"] | config_->shellyCurrentL1;
  config_->shellyCurrentL2 = em["b_current"] | config_->shellyCurrentL2;
  config_->shellyCurrentL3 = em["c_current"] | config_->shellyCurrentL3;
  config_->shellyLastUpdateMs = millis();

  logPrintln("[SHELLY] Live values: L1=" + String(config_->shellyPowerL1, 1) +
             " W / L2=" + String(config_->shellyPowerL2, 1) +
             " W / L3=" + String(config_->shellyPowerL3, 1) +
             " W / total=" + String(config_->shellyTotalPower, 1) +
             " W / voltages=" + String(config_->shellyVoltageL1, 1) + "/" +
             String(config_->shellyVoltageL2, 1) + "/" + String(config_->shellyVoltageL3, 1));
}
