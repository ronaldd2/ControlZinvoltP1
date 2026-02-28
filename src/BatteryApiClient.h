/*
 * BatteryApiClient.h - Common interface for battery cloud backends
 */

#ifndef BATTERYAPICLIENT_H
#define BATTERYAPICLIENT_H

#include <Arduino.h>

class BatteryApiClient {
public:
  virtual ~BatteryApiClient() = default;
  virtual void begin() = 0;
  virtual void loop() = 0;
  virtual void requestFetch(const String& p1Timestamp) = 0;
  virtual String getBackendName() const = 0;
};

#endif // BATTERYAPICLIENT_H
