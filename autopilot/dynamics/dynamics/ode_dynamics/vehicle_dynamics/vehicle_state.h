#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../../state_and_control.h"

namespace celeris {
    // ====== State ======
    struct VehicleOdometryState {
        glm::vec3 position = glm::vec3(0, 0, 0);
        glm::vec3 linear_velocity = glm::vec3(0, 0, 0);

        glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
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

    // ====== State derivative ======
    struct VehicleOdometryStateDerivative {
        glm::vec3 linear_velocity = glm::vec3(0, 0, 0);
        glm::vec3 linear_acceleration = glm::vec3(0, 0, 0);

        glm::vec3 angular_velocity = glm::vec3(0, 0, 0);
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
        glm::vec3 linear_acceleration = glm::vec3(0, 0, 0);
        glm::vec3 angular_velocity = glm::vec3(0, 0, 0);
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

    // ====== Total vehicle state ======
    struct VehicleStateAndControl {
        VehicleState state;
        VehicleControl control;
    };

    using TotalVehicleState = StateAndControl<VehicleState, VehicleControl>;
}
