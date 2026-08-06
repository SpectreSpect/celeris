#pragma once

#include <glm/glm.hpp>

struct VehicleState {
    glm::dvec2 position{0.0};
    double heading = 0.0;
    
    double forward_velocity = 0.0;
    double forward_acceleration = 0.0;

    double steering_angle = 0.0;
    double steering_rate = 0.0;
    double steering_acceleration = 0.0;
};
