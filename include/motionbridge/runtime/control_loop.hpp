#pragma once

#include "motionbridge/control/pid_controller.hpp"
#include "motionbridge/control/trapezoidal_trajectory.hpp"
#include "motionbridge/core/controller_state_machine.hpp"
#include "motionbridge/core/types.hpp"
#include "motionbridge/simulation/servo_plant.hpp"

#include <chrono>
#include <functional>

namespace motionbridge {

class ControlLoop {
public:
    struct Configuration {
        double frequency_hz{1000.0};
        double position_tolerance_rad{0.005};
        double velocity_tolerance_rad_s{0.01};
    };

    using StatusCallback = std::function<void(const ControllerStatus&)>;

    ControlLoop(
        Configuration configuration,
        PidController controller,
        ServoPlant plant = ServoPlant{});

    void set_command(const MotionCommand& command) noexcept;
    void set_external_fault(FaultCode fault) noexcept;
    [[nodiscard]] ControllerStatus step(double dt_seconds) noexcept;

    ControllerStatus run_for(
        std::chrono::duration<double> duration,
        bool real_time = true,
        const StatusCallback& callback = {});

    [[nodiscard]] const ControllerStatus& status() const noexcept;

private:
    [[nodiscard]] FaultCode validate_command(const MotionCommand& command) const noexcept;
    void update_statistics(double execution_us, double jitter_us, bool missed) noexcept;

    Configuration configuration_;
    PidController controller_;
    ServoPlant plant_;
    TrapezoidalTrajectory trajectory_;
    ControllerStateMachine state_machine_;
    MotionCommand command_{};
    FaultCode external_fault_{FaultCode::none};
    ControllerStatus status_{};
    bool motion_initialized_{false};
};

} // namespace motionbridge
