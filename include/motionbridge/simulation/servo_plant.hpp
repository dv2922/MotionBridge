#pragma once

#include "motionbridge/core/types.hpp"

namespace motionbridge {

class ServoPlant {
public:
    struct Parameters {
        double inertia_kg_m2{0.08};
        double viscous_damping_nm_s_rad{0.18};
        double torque_constant_nm_a{0.5};
        double maximum_torque_nm{10.0};
    };

    explicit ServoPlant(Parameters parameters = {});

    const ServoState& update(double torque_command_nm, double dt_seconds) noexcept;
    void reset(double position_rad = 0.0) noexcept;
    [[nodiscard]] const ServoState& state() const noexcept;

private:
    Parameters parameters_;
    ServoState state_{};
};

} // namespace motionbridge

