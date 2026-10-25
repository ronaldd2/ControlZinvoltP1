#ifndef SHELLY_EM3_H
#define SHELLY_EM3_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include "Config.h"

class ShellyEm3Reader {
public:
  ShellyEm3Reader();

  void setConfig(Config* config);
  void begin();
  void loop();

private:
  Config* config_;
  WiFiUDP udp_;
  unsigned long lastPacketMs_ = 0;
  unsigned long lastReportMs_ = 0;

  void parsePayload(const String& payload);
};

#endif
