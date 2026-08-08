#pragma once

#include "dynamic_interface.h"
#include "states/vehicle_state.h"

class AckermannDynamics final : public DynamicInterface<VehicleState> {
public:
    explicit AckermannDynamics(double wheel_base, double max_steering_angle);

    VehicleState& simulate_step_inplace(VehicleState& state, double dt) const override;

private:
    double m_wheel_base = 0.0;
    double m_max_steering_angle = 0.0;
};
