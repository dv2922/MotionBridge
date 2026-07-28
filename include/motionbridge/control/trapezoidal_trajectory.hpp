#pragma once

#include "motionbridge/core/types.hpp"

namespace motionbridge {

class TrapezoidalTrajectory {
public:
    void reset(double position_rad = 0.0) noexcept;

    bool set_target(
        double target_position_rad,
        double max_velocity_rad_s,
        double max_acceleration_rad_s2) noexcept;

    [[nodiscard]] TrajectoryPoint update(double dt_seconds) noexcept;
    [[nodiscard]] TrajectoryPoint current() const noexcept;
    [[nodiscard]] double target() const noexcept;

private:
    double start_position_rad_{0.0};
    double target_position_rad_{0.0};
    double reference_position_rad_{0.0};
    double reference_velocity_rad_s_{0.0};
    double max_velocity_rad_s_{0.0};
    double max_acceleration_rad_s2_{0.0};
    double direction_{1.0};
    double distance_rad_{0.0};
    double peak_velocity_rad_s_{0.0};
    double acceleration_time_s_{0.0};
    double cruise_time_s_{0.0};
    double elapsed_time_s_{0.0};
    double total_time_s_{0.0};
    bool active_{false};
};

} // namespace motionbridge
