#pragma once

namespace motionbridge {

class PidController {
public:
    struct Gains {
        double kp{24.0};
        double ki{4.0};
        double kd{1.2};
        double derivative_filter_time_s{0.01};
    };

    PidController(Gains gains, double minimum_output, double maximum_output);

    [[nodiscard]] double update(
        double reference,
        double measurement,
        double dt_seconds) noexcept;

    void reset() noexcept;

    [[nodiscard]] double integral() const noexcept;

private:
    Gains gains_;
    double minimum_output_;
    double maximum_output_;
    double integral_{0.0};
    double previous_error_{0.0};
    double filtered_derivative_{0.0};
    bool first_update_{true};
};

} // namespace motionbridge

