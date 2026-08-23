"""
DomoticzLogic Optimize Control Simulator
Tests the control algorithm with various scenarios to evaluate performance
"""

import math
import matplotlib.pyplot as plt
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class OptimizeConfig:
    """Configuration parameters for optimize mode"""
    delivery_setpoint: float = 20.0
    min_eva_activity: float = 10.0
    min_solar_power: float = 20.0
    solar_threshold: float = 300.0
    high_solar_setpoint: float = 100.0
    min_delivery_for_adjust: float = 60.0
    adjust_divisor: float = 3.0
    tolerance_low: float = -5.0
    tolerance_high: float = 15.0
    large_error_threshold: float = 200.0
    integrator_reduction: float = 0.66
    hysteresis_delivery: float = 40.0
    hysteresis_adjustment: float = -50.0
    integrator_min: float = -10.0
    integrator_max: float = 10.0
    integrator_step: float = 1.0
    error_deadband: float = 5.0


class DomoticzLogicSimulator:
    """Simulates the DomoticzLogic control algorithm"""
    
    def __init__(self, config: OptimizeConfig):
        self.config = config
        self.integrator = 0.0
        self.delivery_setpoint = config.delivery_setpoint
        self.last_minute_update = 0
        
    def calculate_delivery_setpoint(self, solar: float, soc: float, eva_discharge: float) -> float:
        """Calculate delivery setpoint based on conditions"""
        setpoint = self.config.delivery_setpoint
        
        # Logic for solar-based delivery setpoint
        if eva_discharge < self.config.min_eva_activity and solar > self.config.min_solar_power:
            if solar > self.config.solar_threshold:
                setpoint = self.config.high_solar_setpoint
            else:
                setpoint = self.config.delivery_setpoint + \
                          (self.config.high_solar_setpoint - self.config.delivery_setpoint) * \
                          (solar / self.config.solar_threshold)
        
        return setpoint
    
    def calculate(self, current_p1_delivery: float, solar: float, 
                 eva_charge: float, eva_discharge: float, 
                 soc: float, seconds: int) -> float:
        """Main calculation function - returns power adjustment in watts"""
        
        # Calculate delivery setpoint
        self.delivery_setpoint = self.calculate_delivery_setpoint(solar, soc, eva_discharge)
        
        # Calculate power adjustment
        adjustment = 0.0
        
        if current_p1_delivery > self.config.min_delivery_for_adjust and \
           eva_charge < self.config.min_eva_activity:
            adjustment = current_p1_delivery - self.delivery_setpoint
        else:
            adjustment = (current_p1_delivery - self.delivery_setpoint) / self.config.adjust_divisor
        
        # Within tolerance band - no adjustment needed
        error = current_p1_delivery - self.delivery_setpoint
        if error > self.config.tolerance_low and error < self.config.tolerance_high:
            adjustment = 0.0
        
        # Add integrator contribution
        adjustment += self.integrator
        
        # Reduce integrator if error is large
        if abs(error) > self.config.large_error_threshold:
            self.integrator *= self.config.integrator_reduction
        
        # Handle EVA hysteresis - encourage starting when near zero
        if current_p1_delivery < self.config.hysteresis_delivery and \
           adjustment > self.config.hysteresis_adjustment and \
           adjustment < self.config.min_eva_activity and \
           eva_discharge == 0 and eva_charge == 0:
            adjustment = self.config.hysteresis_adjustment + (seconds % 3) - 1
        
        return math.floor(adjustment)
    
    def update_minute(self, current_p1_delivery: float):
        """Update integrator (call every minute)"""
        error = current_p1_delivery - self.delivery_setpoint
        
        # Update integrator based on error
        if error > self.config.error_deadband:
            self.integrator += self.config.integrator_step
        elif error < -self.config.error_deadband:
            self.integrator -= self.config.integrator_step
        
        # Clamp integrator to prevent wind-up
        self.integrator = max(self.config.integrator_min, 
                             min(self.config.integrator_max, self.integrator))
    
    def reset(self):
        """Reset state"""
        self.integrator = 0.0
        self.delivery_setpoint = self.config.delivery_setpoint


def simulate_scenario(name: str, config: OptimizeConfig, 
                     scenario_generator, duration_minutes: int = 60):
    """
    Simulate a scenario and return results
    
    scenario_generator: function that takes minute and returns 
                       (current_p1_delivery, solar, eva_charge, eva_discharge, soc)
    """
    controller = DomoticzLogicSimulator(config)
    
    results = {
        'time': [],
        'p1_delivery': [],
        'solar': [],
        'eva_charge': [],
        'eva_discharge': [],
        'adjustment': [],
        'setpoint': [],
        'integrator': [],
        'error': []
    }
    
    for minute in range(duration_minutes):
        # Get scenario values
        p1_delivery, solar, eva_charge, eva_discharge, soc = scenario_generator(minute)
        
        # Calculate adjustment
        seconds = minute * 60
        adjustment = controller.calculate(p1_delivery, solar, eva_charge, eva_discharge, soc, seconds)
        
        # Update integrator every minute
        controller.update_minute(p1_delivery)
        
        # Store results
        results['time'].append(minute)
        results['p1_delivery'].append(p1_delivery)
        results['solar'].append(solar)
        results['eva_charge'].append(eva_charge)
        results['eva_discharge'].append(eva_discharge)
        results['adjustment'].append(adjustment)
        results['setpoint'].append(controller.delivery_setpoint)
        results['integrator'].append(controller.integrator)
        results['error'].append(p1_delivery - controller.delivery_setpoint)
    
    return results


def plot_results(scenarios: dict):
    """Plot simulation results for multiple scenarios"""
    num_scenarios = len(scenarios)
    fig, axes = plt.subplots(num_scenarios, 3, figsize=(18, 5 * num_scenarios))
    
    if num_scenarios == 1:
        axes = [axes]
    
    for idx, (name, results) in enumerate(scenarios.items()):
        # Plot 1: Delivery vs Setpoint
        ax1 = axes[idx][0]
        ax1.plot(results['time'], results['p1_delivery'], 'b-', label='P1 Delivery', linewidth=2)
        ax1.plot(results['time'], results['setpoint'], 'r--', label='Setpoint', linewidth=2)
        ax1.fill_between(results['time'], 
                        [s + config.tolerance_low for s in results['setpoint']],
                        [s + config.tolerance_high for s in results['setpoint']],
                        alpha=0.2, color='green', label='Tolerance Band')
        ax1.set_xlabel('Time (minutes)')
        ax1.set_ylabel('Power (W)')
        ax1.set_title(f'{name}\nP1 Delivery vs Setpoint')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Plot 2: Adjustment and Integrator
        ax2 = axes[idx][1]
        ax2.plot(results['time'], results['adjustment'], 'g-', label='Adjustment', linewidth=2)
        ax2_twin = ax2.twinx()
        ax2_twin.plot(results['time'], results['integrator'], 'orange', linestyle='--', 
                     label='Integrator', linewidth=2)
        ax2.set_xlabel('Time (minutes)')
        ax2.set_ylabel('Adjustment (W)', color='g')
        ax2_twin.set_ylabel('Integrator', color='orange')
        ax2.set_title('Control Adjustment & Integrator')
        ax2.legend(loc='upper left')
        ax2_twin.legend(loc='upper right')
        ax2.grid(True, alpha=0.3)
        
        # Plot 3: Solar and Battery Activity
        ax3 = axes[idx][2]
        ax3.plot(results['time'], results['solar'], 'y-', label='Solar', linewidth=2)
        ax3.plot(results['time'], results['eva_charge'], 'b-', label='Battery Charge', linewidth=2)
        ax3.plot(results['time'], results['eva_discharge'], 'r-', label='Battery Discharge', linewidth=2)
        ax3.set_xlabel('Time (minutes)')
        ax3.set_ylabel('Power (W)')
        ax3.set_title('Solar & Battery Activity')
        ax3.legend()
        ax3.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('optimize_control_simulation.png', dpi=150, bbox_inches='tight')
    print("Simulation plot saved as 'optimize_control_simulation.png'")
    plt.show()


def print_statistics(name: str, results: dict):
    """Print statistics for a simulation"""
    errors = results['error']
    adjustments = results['adjustment']
    
    print(f"\n{'='*60}")
    print(f"Scenario: {name}")
    print(f"{'='*60}")
    print(f"Error Statistics:")
    print(f"  Mean Error: {sum(errors)/len(errors):.2f} W")
    print(f"  Max Error: {max(errors):.2f} W")
    print(f"  Min Error: {min(errors):.2f} W")
    print(f"  RMS Error: {math.sqrt(sum(e*e for e in errors)/len(errors)):.2f} W")
    print(f"\nAdjustment Statistics:")
    print(f"  Mean Adjustment: {sum(adjustments)/len(adjustments):.2f} W")
    print(f"  Max Adjustment: {max(adjustments):.2f} W")
    print(f"  Min Adjustment: {min(adjustments):.2f} W")
    
    # Calculate time in tolerance band
    in_tolerance = sum(1 for e in errors if config.tolerance_low < e < config.tolerance_high)
    print(f"\nPerformance:")
    print(f"  Time in tolerance band: {in_tolerance}/{len(errors)} minutes ({100*in_tolerance/len(errors):.1f}%)")


# Test Scenarios
def scenario_stable_solar(minute: int) -> Tuple[float, float, float, float, float]:
    """Stable solar production, battery responding to control"""
    solar = 400.0  # Constant solar (high solar = 100W setpoint)
    # Start near setpoint and add small disturbance
    p1_delivery = 100.0 + 5 * math.sin(minute * 0.2) + minute * 0.1
    eva_charge = 0.0
    eva_discharge = max(0, min(200, abs(p1_delivery - 100) * 2))  # Battery responds to error
    soc = 80.0
    return p1_delivery, solar, eva_charge, eva_discharge, soc


def scenario_varying_solar(minute: int) -> Tuple[float, float, float, float, float]:
    """Varying solar with battery activity"""
    # Solar varies between low (20W setpoint) and high (100W setpoint)
    solar = 200 + 150 * math.sin(minute * math.pi / 30)
    # Setpoint changes with solar: 20W when solar<300, scales to 100W when solar>300
    target_setpoint = 20 + (80 * max(0, min(1, (solar - 20) / 280)))
    # Delivery oscillates around setpoint with some error
    p1_delivery = target_setpoint + 15 * math.sin((minute - 5) * math.pi / 30) + 5
    eva_charge = 0.0
    eva_discharge = max(0, min(200, p1_delivery - target_setpoint + 20))
    soc = 50 + 30 * math.sin(minute * math.pi / 60)
    return p1_delivery, solar, eva_charge, eva_discharge, soc


def scenario_step_change(minute: int) -> Tuple[float, float, float, float, float]:
    """Step changes in delivery to test controller response"""
    solar = 350.0  # High solar = 100W setpoint
    if minute < 15:
        p1_delivery = 100.0  # At setpoint
    elif minute < 30:
        p1_delivery = 130.0  # Step up 30W
    elif minute < 45:
        p1_delivery = 80.0   # Step down to 80W
    else:
        p1_delivery = 110.0  # Step back near setpoint
    
    eva_charge = 0.0
    eva_discharge = max(0, min(300, abs(p1_delivery - 100) * 3))  # Battery responds
    eva_charge = 0.0
    eva_discharge = max(0, p1_delivery - 30)
    soc = 70.0
    return p1_delivery, solar, eva_charge, eva_discharge, soc


def scenario_low_solar_battery_start(minute: int) -> Tuple[float, float, float, float, float]:
    """Low solar, battery needs to start discharging (hysteresis test)"""
    solar = 50.0
    p1_delivery = 15.0 + minute * 0.3  # Slowly increasing
    eva_charge = 0.0
    eva_discharge = 0.0  # Battery not active initially
    soc = 90.0
    return p1_delivery, solar, eva_charge, eva_discharge, soc


def scenario_high_error_recovery(minute: int) -> Tuple[float, float, float, float, float]:
    """High error situation to test integrator reduction"""
    solar = 400.0  # High solar = 100W setpoint
    if minute < 10:
        p1_delivery = 350.0  # Very high delivery (250W error)
    else:
        # Gradually recover to setpoint
        p1_delivery = 100.0 + max(0, 250 - (minute - 10) * 15)
    
    eva_charge = 0.0
    eva_discharge = max(0, min(500, p1_delivery - 100))
    soc = 60.0
    return p1_delivery, solar, eva_charge, eva_discharge, soc


# Main simulation
if __name__ == "__main__":
    # Default configuration
    config = OptimizeConfig()
    
    print("DomoticzLogic Optimize Control Simulation")
    print(f"\nConfiguration:")
    print(f"  Delivery Setpoint: {config.delivery_setpoint} W")
    print(f"  High Solar Setpoint: {config.high_solar_setpoint} W")
    print(f"  Solar Threshold: {config.solar_threshold} W")
    print(f"  Tolerance Band: {config.tolerance_low} to {config.tolerance_high} W")
    print(f"  Integrator Range: {config.integrator_min} to {config.integrator_max}")
    print(f"  Integrator Step: {config.integrator_step}")
    
    # Run scenarios
    scenarios = {
        'Stable Solar Production': simulate_scenario('Stable Solar', config, scenario_stable_solar, 60),
        'Varying Solar Production': simulate_scenario('Varying Solar', config, scenario_varying_solar, 60),
        'Step Changes': simulate_scenario('Step Changes', config, scenario_step_change, 60),
        'Battery Start (Hysteresis)': simulate_scenario('Battery Start', config, scenario_low_solar_battery_start, 60),
        'High Error Recovery': simulate_scenario('High Error', config, scenario_high_error_recovery, 60),
    }
    
    # Print statistics for each scenario
    for name, results in scenarios.items():
        print_statistics(name, results)
    
    # Plot results
    print("\nGenerating plots...")
    plot_results(scenarios)
    
    print("\nSimulation complete!")
