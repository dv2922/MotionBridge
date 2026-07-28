#include "motionbridge/control/pid_controller.hpp"

#include <algorithm>
#include <cmath>

namespace motionbridge {

PidController::PidController(Gains gains, double minimum_output, double maximum_output)
    : gains_(gains)
    , minimum_output_(minimum_output)
    , maximum_output_(maximum_output)
{
}

double PidController::update(
    double reference,
    double measurement,
    double dt_seconds) noexcept
{
    if (!(dt_seconds > 0.0) || !std::isfinite(dt_seconds)) {
        return 0.0;
    }

    const double error = reference - measurement;
    const double raw_derivative = first_update_ ? 0.0 : (error - previous_error_) / dt_seconds;
    const double filter_time = std::max(0.0, gains_.derivative_filter_time_s);
    const double alpha = dt_seconds / (filter_time + dt_seconds);
    filtered_derivative_ += alpha * (raw_derivative - filtered_derivative_);

    const double candidate_integral = integral_ + error * dt_seconds;
    const double unsaturated =
        gains_.kp * error
        + gains_.ki * candidate_integral
        + gains_.kd * filtered_derivative_;
    const double output = std::clamp(unsaturated, minimum_output_, maximum_output_);

    const bool saturated_high = unsaturated > maximum_output_;
    const bool saturated_low = unsaturated < minimum_output_;
    const bool drives_out_of_saturation =
        (saturated_high && error < 0.0) || (saturated_low && error > 0.0);
    if ((!saturated_high && !saturated_low) || drives_out_of_saturation) {
        integral_ = candidate_integral;
    }

    previous_error_ = error;
    first_update_ = false;
    return output;
}

void PidController::reset() noexcept
{
    integral_ = 0.0;
    previous_error_ = 0.0;
    filtered_derivative_ = 0.0;
    first_update_ = true;
}

double PidController::integral() const noexcept
{
    return integral_;
}

} // namespace motionbridge

