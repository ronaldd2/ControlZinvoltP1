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
    if (!config_) return 0.0f;
    
    // Calculate delivery setpoint
    delivery_setpoint_ = calculateDeliverySetpoint(solar, soc, evaDischarge);
    
    // Calculate power adjustment
    float adjustment = 0.0f;
    
    if (currentP1Delivery > config_->optimizeMinDeliveryForAdjust && 
        evaCharge < config_->optimizeMinEvaActivity) {
        adjustment = currentP1Delivery - delivery_setpoint_;
    } else {
        adjustment = (currentP1Delivery - delivery_setpoint_) / config_->optimizeAdjustDivisor;
    }
    
    // Within tolerance band - no adjustment needed
    float error = currentP1Delivery - delivery_setpoint_;
    if (error > config_->optimizeToleranceLow && error < config_->optimizeToleranceHigh) {
        adjustment = 0.0f;
    }
    
    // Add integrator contribution
    adjustment += integrator_;
    
    // Reduce integrator if error is large
    if (abs(error) > config_->optimizeLargeErrorThreshold) {
        integrator_ *= config_->optimizeIntegratorReduction;
    }
    
    // Handle EVA hysteresis - encourage starting when near zero
    if (currentP1Delivery < config_->optimizeHysteresisDelivery && 
        adjustment > config_->optimizeHysteresisAdjustment && 
        adjustment < (config_->optimizeMinEvaActivity) && 
        evaDischarge == 0 && evaCharge == 0) {
        adjustment = config_->optimizeHysteresisAdjustment + (seconds % 3) - 1;
    }
    
    return floor(adjustment);
}

void DomoticzLogic::updateMinute(float currentP1Delivery) {
    if (!config_) return;
    
    float error = currentP1Delivery - delivery_setpoint_;
    
    // Update integrator based on error
    if (error > config_->optimizeErrorDeadband) {
        integrator_ += config_->optimizeIntegratorStep;
    } else if (error < -config_->optimizeErrorDeadband) {
        integrator_ -= config_->optimizeIntegratorStep;
    }
    
    // Clamp integrator to prevent wind-up
    integrator_ = constrain(integrator_, config_->optimizeIntegratorMin, config_->optimizeIntegratorMax);
    
    last_minute_update_ = millis();
}
