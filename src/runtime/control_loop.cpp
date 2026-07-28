#include "motionbridge/runtime/control_loop.hpp"

#include <algorithm>
#include <cmath>
#include <thread>
#include <utility>

namespace motionbridge {

ControlLoop::ControlLoop(
    Configuration configuration,
    PidController controller,
    ServoPlant plant)
    : configuration_(configuration)
    , controller_(std::move(controller))
    , plant_(std::move(plant))
{
    trajectory_.reset(plant_.state().position_rad);
    status_.servo = plant_.state();
    status_.reference = trajectory_.current();
}

void ControlLoop::set_command(const MotionCommand& command) noexcept
{
    command_ = command;
}

void ControlLoop::set_external_fault(FaultCode fault) noexcept
{
    external_fault_ = fault;
}

FaultCode ControlLoop::validate_command(const MotionCommand& command) const noexcept
{
    if (!std::isfinite(command.target_position_rad)
        || !std::isfinite(command.max_velocity_rad_s)
        || !std::isfinite(command.max_acceleration_rad_s2)
        || command.max_velocity_rad_s <= 0.0
        || command.max_acceleration_rad_s2 <= 0.0) {
        return FaultCode::invalid_command;
    }
    return FaultCode::none;
}

ControllerStatus ControlLoop::step(double dt_seconds) noexcept
{
    const ControllerState previous_state = state_machine_.state();
    const FaultCode validation_fault = validate_command(command_);
    const FaultCode detected_fault =
        external_fault_ != FaultCode::none ? external_fault_ : validation_fault;
    state_machine_.update(command_, status_.target_reached, detected_fault);
    const ControllerState current_state = state_machine_.state();

    if (current_state == ControllerState::running
        && (previous_state != ControllerState::running || !motion_initialized_)) {
        trajectory_.reset(plant_.state().position_rad);
        if (!trajectory_.set_target(
                command_.target_position_rad,
                command_.max_velocity_rad_s,
                command_.max_acceleration_rad_s2)) {
            state_machine_.force_fault(FaultCode::invalid_command);
        }
        controller_.reset();
        motion_initialized_ = true;
    }

    double torque_command = 0.0;
    if (state_machine_.state() == ControllerState::running) {
        status_.reference = trajectory_.update(dt_seconds);
        torque_command = controller_.update(
            status_.reference.position_rad,
            plant_.state().position_rad,
            dt_seconds);
    } else {
        controller_.reset();
        motion_initialized_ = false;
        status_.reference = trajectory_.current();
    }

    status_.servo = plant_.update(torque_command, dt_seconds);
    status_.following_error_rad =
        status_.reference.position_rad - status_.servo.position_rad;
    status_.target_reached =
        status_.reference.finished
        && std::abs(command_.target_position_rad - status_.servo.position_rad)
            <= configuration_.position_tolerance_rad
        && std::abs(status_.servo.velocity_rad_s)
            <= configuration_.velocity_tolerance_rad_s;
    status_.state = state_machine_.state();
    status_.fault = state_machine_.fault();
    return status_;
}

ControllerStatus ControlLoop::run_for(
    std::chrono::duration<double> duration,
    bool real_time,
    const StatusCallback& callback)
{
    using Clock = std::chrono::steady_clock;

    if (!(configuration_.frequency_hz > 0.0)) {
        state_machine_.force_fault(FaultCode::invalid_command);
        status_.state = state_machine_.state();
        status_.fault = state_machine_.fault();
        return status_;
    }

    const auto period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>{1.0 / configuration_.frequency_hz});
    const double dt_seconds = 1.0 / configuration_.frequency_hz;
    const auto cycle_count = static_cast<std::uint64_t>(
        std::max(0.0, duration.count()) * configuration_.frequency_hz);
    auto next_cycle = Clock::now();

    for (std::uint64_t cycle = 0; cycle < cycle_count; ++cycle) {
        next_cycle += period;
        const auto started = Clock::now();
        (void)step(dt_seconds);
        const auto completed = Clock::now();

        const double execution_us =
            std::chrono::duration<double, std::micro>(completed - started).count();
        const double jitter_us = real_time
            ? std::abs(std::chrono::duration<double, std::micro>(started - (next_cycle - period)).count())
            : 0.0;
        const bool missed = completed > next_cycle;
        update_statistics(execution_us, jitter_us, missed);

        if (callback) {
            callback(status_);
        }
        if (real_time) {
            std::this_thread::sleep_until(next_cycle);
        }
        if (status_.target_reached && status_.state == ControllerState::ready) {
            break;
        }
    }
    return status_;
}

void ControlLoop::update_statistics(
    double execution_us,
    double jitter_us,
    bool missed) noexcept
{
    auto& timing = status_.timing;
    ++timing.cycles;
    if (missed) {
        ++timing.deadline_misses;
    }
    timing.maximum_execution_us = std::max(timing.maximum_execution_us, execution_us);
    timing.maximum_jitter_us = std::max(timing.maximum_jitter_us, jitter_us);
    timing.mean_execution_us +=
        (execution_us - timing.mean_execution_us) / static_cast<double>(timing.cycles);
}

const ControllerStatus& ControlLoop::status() const noexcept
{
    return status_;
}

} // namespace motionbridge
