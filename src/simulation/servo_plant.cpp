#include "motionbridge/simulation/servo_plant.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace motionbridge {

ServoPlant::ServoPlant(Parameters parameters)
    : parameters_(parameters)
{
    if (!(parameters_.inertia_kg_m2 > 0.0)
        || !(parameters_.torque_constant_nm_a > 0.0)
        || !(parameters_.maximum_torque_nm > 0.0)) {
        throw std::invalid_argument("ServoPlant parameters must be positive");
    }
}

const ServoState& ServoPlant::update(
    double torque_command_nm,
    double dt_seconds) noexcept
{
    if (!(dt_seconds > 0.0) || !std::isfinite(dt_seconds)) {
        return state_;
    }

    const double torque = std::clamp(
        torque_command_nm,
        -parameters_.maximum_torque_nm,
        parameters_.maximum_torque_nm);
    const double acceleration =
        (torque - parameters_.viscous_damping_nm_s_rad * state_.velocity_rad_s)
        / parameters_.inertia_kg_m2;

    state_.velocity_rad_s += acceleration * dt_seconds;
    state_.position_rad += state_.velocity_rad_s * dt_seconds;
    state_.commanded_torque_nm = torque;
    state_.simulated_current_a = torque / parameters_.torque_constant_nm_a;
    return state_;
}

void ServoPlant::reset(double position_rad) noexcept
{
    state_ = {};
    state_.position_rad = position_rad;
}

const ServoState& ServoPlant::state() const noexcept
{
    return state_;
}

} // namespace motionbridge

