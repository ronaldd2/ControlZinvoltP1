#include "DomoticzLogic.h"
#include "Config.h"

DomoticzLogic::DomoticzLogic() 
    : config_(nullptr),
      integrator_(0.0f),
      delivery_setpoint_(20.0f),
      last_minute_update_(0) {
}

void DomoticzLogic::reset() {
    integrator_ = 0.0f;
    delivery_setpoint_ = 20.0f;
    last_minute_update_ = 0;
}

float DomoticzLogic::calculateDeliverySetpoint(float solar, float soc, 
                                              float evaDischarge) {
    if (!config_) return 20.0f;
    
    float setpoint = config_->optimizeDeliverySetpoint;
    
    // Default logic for solar-based delivery setpoint
    if (evaDischarge < config_->optimizeMinEvaActivity && solar > config_->optimizeMinSolarPower) {
        if (solar > config_->optimizeSolarThreshold) {
            setpoint = config_->optimizeHighSolarSetpoint;
        } else {
            setpoint = config_->optimizeDeliverySetpoint + 
                      (config_->optimizeHighSolarSetpoint - config_->optimizeDeliverySetpoint) * 
                      (solar / config_->optimizeSolarThreshold);
        }
    }
    
    return setpoint;
}

float DomoticzLogic::calculate(float currentP1Delivery, float solar, 
                              float evaCharge, float evaDischarge, 
                              float soc, int seconds) {
    (void)seconds;
    if (!config_) return 0.0f;
    
    // Calculate delivery setpoint
    delivery_setpoint_ = calculateDeliverySetpoint(solar, soc, evaDischarge);
    
    // Calculate power adjustment
    float adjustment = 0.0f;
    
    float error = currentP1Delivery - delivery_setpoint_;
    float divisor = max(1.0f, config_->optimizeAdjustDivisor);

    if (evaCharge < config_->optimizeMinEvaActivity) {
        adjustment = error;
    } else {
        adjustment = error / divisor;
    }

    // Small fixed deadband to avoid noise-driven oscillation.
    if (fabs(error) <= 5.0f) {
        adjustment = 0.0f;
    }
    
    // Add integrator contribution
    adjustment += integrator_;
    
    return floor(adjustment);
}

void DomoticzLogic::updateMinute(float currentP1Delivery) {
    if (!config_) return;
    
    float error = currentP1Delivery - delivery_setpoint_;
    const float deadbandW = 5.0f;
    const float integratorStep = 1.0f;
    
    // Update integrator based on error
    if (error > deadbandW) {
        integrator_ += integratorStep;
    } else if (error < -deadbandW) {
        integrator_ -= integratorStep;
    }
    
    // Clamp integrator to prevent wind-up
    integrator_ = constrain(integrator_, -10.0f, 10.0f);
    
    last_minute_update_ = millis();
}
