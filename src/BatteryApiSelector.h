/*
 * BatteryApiSelector.h - Selects active battery backend client
 */

#ifndef BATTERYAPISELECTOR_H
#define BATTERYAPISELECTOR_H

#include <Arduino.h>
#include "Config.h"
#include "BatteryApiClient.h"
#include "AlphaESSClient.h"
#include "ZinvoltClient.h"

class BatteryApiSelector : public BatteryApiClient {
public:
  BatteryApiSelector(Config* config);

  void begin() override;
  void loop() override;
  void requestFetch(const String& p1Timestamp) override;
  String getBackendName() const override;

private:
  Config* config_;
  AlphaESSClient alphaEssClient_;
  ZinvoltClient zinvoltClient_;

  BatteryApiClient* getActiveClient();
  const BatteryApiClient* getActiveClient() const;
};

#endif // BATTERYAPISELECTOR_H
