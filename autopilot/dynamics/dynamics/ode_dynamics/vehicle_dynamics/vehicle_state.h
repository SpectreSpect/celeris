#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>

#include "../../state_and_control.h"

namespace celeris {
    // ====== State ======
    struct VehicleOdometryState {
        glm::dvec3 position{0.0};
        glm::dvec3 linear_velocity{0.0};

        glm::dquat orientation{1.0, 0.0, 0.0, 0.0};
    };

    struct VehicleSteeringWheelState {
        double steering_angle = 0.0;
        double steering_rate = 0.0;
    };

    //-------------------------------------------------------
    struct VehicleState {
        VehicleOdometryState odometry;
        VehicleSteeringWheelState steering_wheel;
    };
    //-------------------------------------------------------

    // ====== Partial state replacement ======
    struct VehicleOdometryStatePatch {
        std::optional<glm::dvec3> position{};
        std::optional<glm::dvec3> linear_velocity{};

        std::optional<glm::dquat> orientation{};
    };

    struct VehicleSteeringWheelStatePatch {
        std::optional<double> steering_angle{};
        std::optional<double> steering_rate{};
    };

    //-------------------------------------------------------
    struct VehicleStatePatch {
        std::optional<VehicleOdometryStatePatch> odometry{};
        std::optional<VehicleSteeringWheelStatePatch> steering_wheel{};
    };
    //-------------------------------------------------------

    // ====== State derivative ======
    struct VehicleOdometryStateDerivative {
        glm::dvec3 linear_velocity{0.0};
        glm::dvec3 linear_acceleration{0.0};

        glm::dquat orientation_derivative{0.0, 0.0, 0.0, 0.0};
    };

    struct VehicleSteeringWheelStateDerivative {
        double steering_rate = 0.0;
        double steering_acceleration = 0.0;
    };

    struct VehicleStateDerivative {
        VehicleOdometryStateDerivative odometry_derivative;
        VehicleSteeringWheelStateDerivative steering_wheel_derivative;
    };

    // ====== Control ======
    struct VehicleOdometryControl {
        glm::dvec3 linear_acceleration{0.0};
        glm::dvec3 angular_velocity{0.0};
    };

    struct VehicleSteeringWheelControl {
        double steering_acceleration = 0.0;
    };

    //-------------------------------------------------------
    struct VehicleControl {
        VehicleOdometryControl odometry;
        VehicleSteeringWheelControl steering_wheel;
    };
    //-------------------------------------------------------

    // ====== Partial control replacement ======
    struct VehicleOdometryControlPatch {
        std::optional<glm::dvec3> linear_acceleration{};
        std::optional<glm::dvec3> angular_velocity{};
    };

    struct VehicleSteeringWheelControlPatch {
        std::optional<double> steering_acceleration{};
    };

    //-------------------------------------------------------
    struct VehicleControlPatch {
        std::optional<VehicleOdometryControlPatch> odometry{};
        std::optional<VehicleSteeringWheelControlPatch> steering_wheel{};
    };
    //-------------------------------------------------------

    // ====== Total vehicle state ======
    using TotalVehicleState = StateAndControl<VehicleState, VehicleControl>;

    struct TotalVehicleStatePatch {
        std::optional<VehicleStatePatch> state{};
        std::optional<VehicleControlPatch> control{};
    };

    inline VehicleState operator*(double scalar, const VehicleStateDerivative& derivative) {
        return VehicleState{
            .odometry = VehicleOdometryState{
                .position = scalar * derivative.odometry_derivative.linear_velocity,
                .linear_velocity = scalar * derivative.odometry_derivative.linear_acceleration,
                .orientation = scalar * derivative.odometry_derivative.orientation_derivative
            },
            .steering_wheel = VehicleSteeringWheelState{
                .steering_angle = scalar * derivative.steering_wheel_derivative.steering_rate,
                .steering_rate = scalar * derivative.steering_wheel_derivative.steering_acceleration
            }
        };
    }

    inline VehicleState operator*(const VehicleStateDerivative& derivative, double scalar) {
        return scalar * derivative;
    }

    inline VehicleStateDerivative operator+(const VehicleStateDerivative& lhs_derivative, const VehicleStateDerivative& rhs_derivative) {
        return VehicleStateDerivative{
            .odometry_derivative = VehicleOdometryStateDerivative{
                .linear_velocity = lhs_derivative.odometry_derivative.linear_velocity + rhs_derivative.odometry_derivative.linear_velocity,
                .linear_acceleration = lhs_derivative.odometry_derivative.linear_acceleration + rhs_derivative.odometry_derivative.linear_acceleration,
                .orientation_derivative = lhs_derivative.odometry_derivative.orientation_derivative + rhs_derivative.odometry_derivative.orientation_derivative
            },
            .steering_wheel_derivative = VehicleSteeringWheelStateDerivative{
                .steering_rate = lhs_derivative.steering_wheel_derivative.steering_rate + rhs_derivative.steering_wheel_derivative.steering_rate,
                .steering_acceleration = 
                    lhs_derivative.steering_wheel_derivative.steering_acceleration + rhs_derivative.steering_wheel_derivative.steering_acceleration
            }
        };
    }

    inline VehicleState operator+(const VehicleState& lhs_state, const VehicleState& rhs_state) {
        return VehicleState{
            .odometry = VehicleOdometryState{
                .position = lhs_state.odometry.position + rhs_state.odometry.position,
                .linear_velocity = lhs_state.odometry.linear_velocity + rhs_state.odometry.linear_velocity,
                .orientation = lhs_state.odometry.orientation + rhs_state.odometry.orientation
            },
            .steering_wheel = VehicleSteeringWheelState{
                .steering_angle = lhs_state.steering_wheel.steering_angle + rhs_state.steering_wheel.steering_angle,
                .steering_rate = lhs_state.steering_wheel.steering_rate + rhs_state.steering_wheel.steering_rate
            }
        };
    }

    inline VehicleState& operator+=(VehicleState& lhs_state, const VehicleState& rhs_state) {
        lhs_state.odometry.position += rhs_state.odometry.position;
        lhs_state.odometry.linear_velocity += rhs_state.odometry.linear_velocity;
        lhs_state.odometry.orientation += rhs_state.odometry.orientation;

        lhs_state.steering_wheel.steering_angle += rhs_state.steering_wheel.steering_angle;
        lhs_state.steering_wheel.steering_rate += rhs_state.steering_wheel.steering_rate;

        return lhs_state;
    }

    inline VehicleState& operator^=(VehicleState& state, const VehicleStatePatch& patch) {
        if (patch.odometry.has_value()) {
            const VehicleOdometryStatePatch& odometry_patch = *patch.odometry;

            if (odometry_patch.position.has_value()) {
                state.odometry.position = *odometry_patch.position;
            }
            if (odometry_patch.linear_velocity.has_value()) {
                state.odometry.linear_velocity = *odometry_patch.linear_velocity;
            }
            if (odometry_patch.orientation.has_value()) {
                state.odometry.orientation = *odometry_patch.orientation;
            }
        }

        if (patch.steering_wheel.has_value()) {
            const VehicleSteeringWheelStatePatch& steering_patch = *patch.steering_wheel;

            if (steering_patch.steering_angle.has_value()) {
                state.steering_wheel.steering_angle = *steering_patch.steering_angle;
            }
            if (steering_patch.steering_rate.has_value()) {
                state.steering_wheel.steering_rate = *steering_patch.steering_rate;
            }
        }

        return state;
    }

    inline VehicleControl& operator^=(VehicleControl& control, const VehicleControlPatch& patch) {
        if (patch.odometry.has_value()) {
            const VehicleOdometryControlPatch& odometry_patch = *patch.odometry;

            if (odometry_patch.linear_acceleration.has_value()) {
                control.odometry.linear_acceleration = *odometry_patch.linear_acceleration;
            }
            if (odometry_patch.angular_velocity.has_value()) {
                control.odometry.angular_velocity = *odometry_patch.angular_velocity;
            }
        }

        if (patch.steering_wheel.has_value()) {
            const VehicleSteeringWheelControlPatch& steering_patch = *patch.steering_wheel;

            if (steering_patch.steering_acceleration.has_value()) {
                control.steering_wheel.steering_acceleration = *steering_patch.steering_acceleration;
            }
        }

        return control;
    }

    inline TotalVehicleState& operator^=(TotalVehicleState& total_state, const TotalVehicleStatePatch& patch) {
        if (patch.state.has_value()) {
            total_state.state ^= *patch.state;
        }
        
        if (patch.control.has_value()) {
            total_state.control ^= *patch.control;
        }

        return total_state;
    }
}
