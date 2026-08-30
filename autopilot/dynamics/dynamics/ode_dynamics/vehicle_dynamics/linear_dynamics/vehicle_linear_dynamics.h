#pragma once

#include <utility>

#include "../../../../integrators/ode_rk2_integrator.h"
#include "../../controlled_ode_adapter.h"
#include "../vehicle_state.h"
#include "vehicle_linear_equation.h"

namespace celeris {
    template<class Integrator>
    class VehicleLinearDynamics : public ControlledOdeAdapter<
        VehicleState,
        VehicleStateDerivative,
        VehicleControl,
        VehicleLinearEquation,
        Integrator>
    {
    public:
        explicit VehicleLinearDynamics(Integrator integrator)
            :   ControlledOdeAdapter<
                    VehicleState,
                    VehicleStateDerivative,
                    VehicleControl,
                    VehicleLinearEquation,
                    Integrator
                >(
                    VehicleLinearEquation(),
                    std::move(integrator)
                ) {}
    };

    class VehicleLinearDynamicsRk2 final : public VehicleLinearDynamics<
        OdeRk2Integrator<VehicleState, VehicleStateDerivative, VehicleControl>>
    {
    public:
        VehicleLinearDynamicsRk2();
    };
}
