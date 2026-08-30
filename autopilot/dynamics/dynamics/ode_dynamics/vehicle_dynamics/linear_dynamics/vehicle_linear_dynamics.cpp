#include "vehicle_linear_dynamics.h"

namespace celeris {
    VehicleLinearDynamicsRk2::VehicleLinearDynamicsRk2() 
        :   VehicleLinearDynamics<
                OdeRk2Integrator<VehicleState, VehicleStateDerivative, VehicleControl>
            >( 
                OdeRk2Integrator<VehicleState, VehicleStateDerivative, VehicleControl>()
            ) {}
}
