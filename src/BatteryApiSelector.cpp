/*
 * BatteryApiSelector.cpp - Selects active battery backend implementation
 */

#include "BatteryApiSelector.h"

BatteryApiSelector::BatteryApiSelector(Config* config)
  : config_(config),
    alphaEssClient_(config),
    zinvoltClient_(config) {
}

void BatteryApiSelector::begin() {
  alphaEssClient_.begin();
  zinvoltClient_.begin();
}

void BatteryApiSelector::loop() {
  getActiveClient()->loop();
}

void BatteryApiSelector::requestFetch(const String& p1Timestamp) {
  getActiveClient()->requestFetch(p1Timestamp);
}

String BatteryApiSelector::getBackendName() const {
  return getActiveClient()->getBackendName();
}

BatteryApiClient* BatteryApiSelector::getActiveClient() {
  String backend = config_->batteryBackend;
  backend.toLowerCase();
  if (backend == "zinvolt") {
    return &zinvoltClient_;
  }
  return &alphaEssClient_;
}

const BatteryApiClient* BatteryApiSelector::getActiveClient() const {
  String backend = config_->batteryBackend;
  backend.toLowerCase();
  if (backend == "zinvolt") {
    return &zinvoltClient_;
  }
  return &alphaEssClient_;
}
