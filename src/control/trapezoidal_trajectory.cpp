#include "motionbridge/control/trapezoidal_trajectory.hpp"

#include <algorithm>
#include <cmath>

namespace motionbridge {

void TrapezoidalTrajectory::reset(double position_rad) noexcept
{
    start_position_rad_ = position_rad;
    target_position_rad_ = position_rad;
    reference_position_rad_ = position_rad;
    reference_velocity_rad_s_ = 0.0;
    direction_ = 1.0;
    distance_rad_ = 0.0;
    peak_velocity_rad_s_ = 0.0;
    acceleration_time_s_ = 0.0;
    cruise_time_s_ = 0.0;
    elapsed_time_s_ = 0.0;
    total_time_s_ = 0.0;
    active_ = false;
}

bool TrapezoidalTrajectory::set_target(
    double target_position_rad,
    double max_velocity_rad_s,
    double max_acceleration_rad_s2) noexcept
{
    if (!std::isfinite(target_position_rad)
        || !std::isfinite(max_velocity_rad_s)
        || !std::isfinite(max_acceleration_rad_s2)
        || max_velocity_rad_s <= 0.0
        || max_acceleration_rad_s2 <= 0.0) {
        return false;
    }

    start_position_rad_ = reference_position_rad_;
    target_position_rad_ = target_position_rad;
    max_velocity_rad_s_ = max_velocity_rad_s;
    max_acceleration_rad_s2_ = max_acceleration_rad_s2;
    const double displacement = target_position_rad_ - start_position_rad_;
    direction_ = displacement >= 0.0 ? 1.0 : -1.0;
    distance_rad_ = std::abs(displacement);
    elapsed_time_s_ = 0.0;

    acceleration_time_s_ = max_velocity_rad_s_ / max_acceleration_rad_s2_;
    const double acceleration_distance =
        0.5 * max_acceleration_rad_s2_ * acceleration_time_s_ * acceleration_time_s_;
    if (2.0 * acceleration_distance >= distance_rad_) {
        acceleration_time_s_ = std::sqrt(distance_rad_ / max_acceleration_rad_s2_);
        peak_velocity_rad_s_ = max_acceleration_rad_s2_ * acceleration_time_s_;
        cruise_time_s_ = 0.0;
    } else {
        peak_velocity_rad_s_ = max_velocity_rad_s_;
        cruise_time_s_ =
            (distance_rad_ - 2.0 * acceleration_distance) / peak_velocity_rad_s_;
    }
    total_time_s_ = 2.0 * acceleration_time_s_ + cruise_time_s_;
    active_ = distance_rad_ > 1e-12;
    return true;
}

TrajectoryPoint TrapezoidalTrajectory::update(double dt_seconds) noexcept
{
    if (!active_ || !(dt_seconds > 0.0) || !std::isfinite(dt_seconds)) {
        return current();
    }

    elapsed_time_s_ = std::min(elapsed_time_s_ + dt_seconds, total_time_s_);
    const double acceleration_distance =
        0.5 * max_acceleration_rad_s2_ * acceleration_time_s_ * acceleration_time_s_;
    const double cruise_distance = peak_velocity_rad_s_ * cruise_time_s_;
    double profile_position = 0.0;
    double profile_velocity = 0.0;

    if (elapsed_time_s_ < acceleration_time_s_) {
        profile_position =
            0.5 * max_acceleration_rad_s2_ * elapsed_time_s_ * elapsed_time_s_;
        profile_velocity = max_acceleration_rad_s2_ * elapsed_time_s_;
    } else if (elapsed_time_s_ < acceleration_time_s_ + cruise_time_s_) {
        const double cruise_elapsed = elapsed_time_s_ - acceleration_time_s_;
        profile_position = acceleration_distance + peak_velocity_rad_s_ * cruise_elapsed;
        profile_velocity = peak_velocity_rad_s_;
    } else if (elapsed_time_s_ < total_time_s_) {
        const double deceleration_elapsed =
            elapsed_time_s_ - acceleration_time_s_ - cruise_time_s_;
        profile_position =
            acceleration_distance
            + cruise_distance
            + peak_velocity_rad_s_ * deceleration_elapsed
            - 0.5 * max_acceleration_rad_s2_
                * deceleration_elapsed * deceleration_elapsed;
        profile_velocity =
            peak_velocity_rad_s_ - max_acceleration_rad_s2_ * deceleration_elapsed;
    } else {
        reference_position_rad_ = target_position_rad_;
        reference_velocity_rad_s_ = 0.0;
        active_ = false;
        return current();
    }

    reference_position_rad_ = start_position_rad_ + direction_ * profile_position;
    reference_velocity_rad_s_ = direction_ * profile_velocity;
    return current();
}

TrajectoryPoint TrapezoidalTrajectory::current() const noexcept
{
    return {
        reference_position_rad_,
        reference_velocity_rad_s_,
        !active_
    };
}

double TrapezoidalTrajectory::target() const noexcept
{
    return target_position_rad_;
}

} // namespace motionbridge
