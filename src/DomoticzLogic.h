#ifndef DOMOTICZ_LOGIC_H
#define DOMOTICZ_LOGIC_H

#include <Arduino.h>

class Config;  // Forward declaration

class DomoticzLogic {
public:
    DomoticzLogic();
    
    // Set config pointer
    void setConfig(Config* config) { config_ = config; }
    
    // Main calculation function - returns power adjustment in watts
    float calculate(float currentP1Delivery, float solar, float evaCharge, 
                   float evaDischarge, float soc, int seconds);
    
    // Update integrator (call every minute)
    void updateMinute(float currentP1Delivery);
    
    // Reset state
    void reset();
    
    // Getters for state
    float getDeliverySetpoint() const { return delivery_setpoint_; }
    float getIntegrator() const { return integrator_; }
    
private:
    Config* config_;  // Pointer to config for accessing settings
    
    // State variables (now class members)
    float integrator_;
    float delivery_setpoint_;
    unsigned long last_minute_update_;
    
    // Calculate delivery setpoint based on conditions
    float calculateDeliverySetpoint(float solar, float soc, float evaDischarge);
};

#endif // DOMOTICZ_LOGIC_H
