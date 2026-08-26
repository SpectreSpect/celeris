#pragma once

#include "../../../../integrators/ode_equation_interface.h"
#include "../../../../clock.h"
#include "../vehicle_state.h"

namespace celeris {
    class VehicleLinearEquation final : public OdeEquationInterface<
        VehicleState, 
        VehicleStateDerivative, 
        VehicleControl> 
    {
    public:
        [[nodiscard]]
        VehicleStateDerivative calculate_derivative(
            simulation::Timestamp timestamp,
            const VehicleState& state,
            const VehicleControl& control) const override
        {
            LOG_METHOD();
            return 
            {
                .odometry_derivative = VehicleOdometryStateDerivative{
                    .linear_velocity = state.odometry.linear_velocity,
                    .linear_acceleration = control.odometry.linear_acceleration,
                    .angular_velocity = control.odometry.angular_velocity
                },
                .steering_wheel_derivative = VehicleSteeringWheelStateDerivative {
                    .steering_rate = state.steering_wheel.steering_rate,
                    .steering_acceleration = control.steering_wheel.steering_acceleration
                }
            };
        }
    };
}
