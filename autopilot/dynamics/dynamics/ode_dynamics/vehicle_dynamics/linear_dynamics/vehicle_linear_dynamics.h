#pragma once

#include "../../../../integrators/ode_rk2_integrator.h"
#include "../../controlled_ode_adapter.h"
#include "../vehicle_state.h"
#include "vehicle_linear_equation.h"

namespace celeris {
    using VehicleLinearDynamics = ControlledOdeAdapter<
        VehicleState, // State
        VehicleStateDerivative, // Derivative
        VehicleControl, // Control
        VehicleLinearEquation, // Equation
        OdeRk2Integrator<VehicleState, VehicleStateDerivative, VehicleControl> // Integrator
    >;
}
