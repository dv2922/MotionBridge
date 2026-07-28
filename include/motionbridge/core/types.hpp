#pragma once

#include <cstdint>
#include <string_view>

namespace motionbridge {

enum class ControllerState : std::uint8_t {
    power_off,
    disabled,
    ready,
    running,
    stopping,
    fault,
    emergency_stop
};

enum class FaultCode : std::uint16_t {
    none = 0,
    communication_timeout = 1001,
    position_limit_exceeded = 1002,
    velocity_limit_exceeded = 1003,
    following_error_exceeded = 1004,
    control_loop_overrun = 1005,
    invalid_command = 1006
};

struct MotionCommand {
    bool power_available{true};
    bool enable{false};
    bool start{false};
    bool stop{false};
    bool reset_fault{false};
    bool emergency_stop{false};
    double target_position_rad{0.0};
    double max_velocity_rad_s{0.8};
    double max_acceleration_rad_s2{1.5};
};

struct ServoState {
    double position_rad{0.0};
    double velocity_rad_s{0.0};
    double commanded_torque_nm{0.0};
    double simulated_current_a{0.0};
};

struct TrajectoryPoint {
    double position_rad{0.0};
    double velocity_rad_s{0.0};
    bool finished{true};
};

struct LoopStatistics {
    std::uint64_t cycles{0};
    std::uint64_t deadline_misses{0};
    double mean_execution_us{0.0};
    double maximum_execution_us{0.0};
    double maximum_jitter_us{0.0};
};

struct ControllerStatus {
    ControllerState state{ControllerState::power_off};
    FaultCode fault{FaultCode::none};
    ServoState servo{};
    TrajectoryPoint reference{};
    double following_error_rad{0.0};
    LoopStatistics timing{};
    bool target_reached{false};
};

[[nodiscard]] constexpr std::string_view to_string(ControllerState state) noexcept
{
    switch (state) {
    case ControllerState::power_off:
        return "POWER_OFF";
    case ControllerState::disabled:
        return "DISABLED";
    case ControllerState::ready:
        return "READY";
    case ControllerState::running:
        return "RUNNING";
    case ControllerState::stopping:
        return "STOPPING";
    case ControllerState::fault:
        return "FAULT";
    case ControllerState::emergency_stop:
        return "EMERGENCY_STOP";
    }
    return "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view to_string(FaultCode fault) noexcept
{
    switch (fault) {
    case FaultCode::none:
        return "NONE";
    case FaultCode::communication_timeout:
        return "COMMUNICATION_TIMEOUT";
    case FaultCode::position_limit_exceeded:
        return "POSITION_LIMIT_EXCEEDED";
    case FaultCode::velocity_limit_exceeded:
        return "VELOCITY_LIMIT_EXCEEDED";
    case FaultCode::following_error_exceeded:
        return "FOLLOWING_ERROR_EXCEEDED";
    case FaultCode::control_loop_overrun:
        return "CONTROL_LOOP_OVERRUN";
    case FaultCode::invalid_command:
        return "INVALID_COMMAND";
    }
    return "UNKNOWN";
}

} // namespace motionbridge

