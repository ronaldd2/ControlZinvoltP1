# Optimize Control Simulation & Testing

This directory contains tools to simulate and test the DomoticzLogic optimize control algorithm without requiring actual hardware.

## Files

- `test_optimize_control.py` - Python simulation script that tests the control algorithm with various scenarios

## Requirements

```bash
pip install matplotlib
```

## Running the Simulation

```bash
python test_optimize_control.py
```

This will:
1. Run 5 different test scenarios
2. Print performance statistics for each scenario
3. Generate plots showing:
   - P1 Delivery vs Setpoint with tolerance band
   - Control Adjustment and Integrator behavior
   - Solar and Battery Activity
4. Save the plot as `optimize_control_simulation.png`

## Test Scenarios

### 1. Stable Solar Production
- Constant 400W solar production
- Gradually increasing grid delivery
- Tests steady-state tracking

### 2. Varying Solar Production
- Sinusoidal solar variation (50-350W)
- Delivery follows solar with phase lag
- Tests dynamic response

### 3. Step Changes
- Large step changes in delivery (100W → 200W → 50W → 150W)
- Tests transient response and overshoot
- Evaluates settling time

### 4. Battery Start (Hysteresis)
- Low delivery, battery not active
- Tests hysteresis mechanism to encourage battery activation
- Critical for avoiding oscillations

### 5. High Error Recovery
- Initial very high error (500W delivery)
- Tests integrator wind-up prevention
- Tests integrator reduction mechanism

## Performance Metrics

For each scenario, the simulation reports:

- **Error Statistics**: Mean, Max, Min, RMS error from setpoint
- **Adjustment Statistics**: Control output characteristics
- **Time in Tolerance Band**: Percentage of time within acceptable range

## Customizing the Simulation

You can modify the configuration in the script:

```python
config = OptimizeConfig(
    delivery_setpoint=20.0,
    high_solar_setpoint=100.0,
    solar_threshold=300.0,
    tolerance_low=-5.0,
    tolerance_high=15.0,
    integrator_min=-10.0,
    integrator_max=10.0,
    integrator_step=1.0,
    error_deadband=5.0,
    # ... other parameters
)
```

## Adding New Scenarios

Create a new scenario generator function:

```python
def scenario_my_test(minute: int) -> Tuple[float, float, float, float, float]:
    """Description of the scenario"""
    p1_delivery = ...  # Your logic
    solar = ...
    eva_charge = ...
    eva_discharge = ...
    soc = ...
    return p1_delivery, solar, eva_charge, eva_discharge, soc
```

Then add it to the scenarios dict:

```python
scenarios = {
    'My Test': simulate_scenario('My Test', config, scenario_my_test, 60),
    # ... existing scenarios
}
```

## Interpreting Results

### Good Performance Indicators:
- ✅ High percentage of time in tolerance band (>80%)
- ✅ Low RMS error (<30W)
- ✅ Smooth adjustment curve (no oscillations)
- ✅ Integrator stays within bounds
- ✅ Fast recovery from disturbances (<5 minutes)

### Warning Signs:
- ⚠️ Oscillations in adjustment
- ⚠️ Integrator hitting limits frequently
- ⚠️ Large overshoots (>100W)
- ⚠️ Slow settling time (>10 minutes)

## Tuning Parameters

If you see issues, try adjusting:

1. **Oscillations** → Increase `adjust_divisor` or widen tolerance band
2. **Slow response** → Decrease `adjust_divisor` or increase `integrator_step`
3. **Overshoot** → Reduce `integrator_step` or `high_solar_setpoint`
4. **Integrator saturation** → Widen `integrator_min`/`integrator_max` or reduce `integrator_step`
5. **Battery not starting** → Adjust `hysteresis_delivery` and `hysteresis_adjustment`

## Next Steps

After simulation testing:
1. Deploy parameters to device via web UI
2. Monitor actual behavior in `/Actuals` page
3. Fine-tune based on real-world performance
4. Use Telnet logs (port 23) to see detailed control actions
