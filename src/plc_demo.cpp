#include "motionbridge/communication/mock_plc.hpp"
#include "motionbridge/runtime/plc_supervisor.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

void announce(double time_s, const char* event)
{
    std::cout << std::fixed << std::setprecision(3)
              << '[' << time_s << " s] PLC: " << event << '\n';
}

} // namespace

int main()
{
    constexpr double dt = 0.001;
    constexpr std::uint64_t total_cycles = 1800;

    motionbridge::PidController pid{
        {.kp = 32.0, .ki = 5.0, .kd = 2.0, .derivative_filter_time_s = 0.008},
        -10.0,
        10.0};
    motionbridge::ControlLoop control_loop{
        {.frequency_hz = 1000.0,
         .position_tolerance_rad = 0.005,
         .velocity_tolerance_rad_s = 0.01},
        pid};
    motionbridge::MockPlc plc;
    (void)plc.connect();
    motionbridge::PlcSupervisor supervisor{
        {.poll_period_seconds = 0.02, .watchdog_timeout_seconds = 0.10},
        plc,
        control_loop};

    motionbridge::PlcCommandData command;
    command.target_position_rad = 2.0;
    command.max_velocity_rad_s = 0.8;
    command.max_acceleration_rad_s2 = 1.5;
    command.heartbeat = 1;
    plc.set_command(command);

    auto previous_state = motionbridge::ControllerState::power_off;
    for (std::uint64_t cycle = 0; cycle < total_cycles; ++cycle) {
        const double time_s = static_cast<double>(cycle) * dt;

        if (cycle == 20) {
            command.control_word |= motionbridge::plc_control_bit::enable;
            ++command.heartbeat;
            plc.set_command(command);
            announce(time_s, "enable axis");
        } else if (cycle == 60) {
            command.control_word |= motionbridge::plc_control_bit::start;
            ++command.heartbeat;
            plc.set_command(command);
            announce(time_s, "start move to 2.0 rad");
        } else if (cycle == 80) {
            command.control_word &= static_cast<std::uint16_t>(
                ~motionbridge::plc_control_bit::start);
            ++command.heartbeat;
            plc.set_command(command);
        } else if (cycle > 80 && cycle < 1000 && cycle % 20 == 0) {
            ++command.heartbeat;
            plc.set_command(command);
        } else if (cycle == 1000) {
            announce(time_s, "heartbeat frozen (simulated network loss)");
        } else if (cycle == 1300) {
            command.control_word = static_cast<std::uint16_t>(
                motionbridge::plc_control_bit::power_available
                | motionbridge::plc_control_bit::reset_fault);
            ++command.heartbeat;
            plc.set_command(command);
            announce(time_s, "communication restored and fault reset requested");
        } else if (cycle == 1340) {
            command.control_word = static_cast<std::uint16_t>(
                motionbridge::plc_control_bit::power_available
                | motionbridge::plc_control_bit::enable);
            ++command.heartbeat;
            plc.set_command(command);
            announce(time_s, "axis enabled again");
        } else if (cycle > 1300 && cycle % 20 == 0) {
            ++command.heartbeat;
            plc.set_command(command);
        }

        const auto status = supervisor.update(dt);
        if (status.state != previous_state) {
            std::cout << '[' << time_s << " s] Controller: "
                      << motionbridge::to_string(previous_state) << " -> "
                      << motionbridge::to_string(status.state);
            if (status.fault != motionbridge::FaultCode::none) {
                std::cout << " (" << motionbridge::to_string(status.fault) << ')';
            }
            std::cout << '\n';
            previous_state = status.state;
        }
    }

    const auto& final_status = control_loop.status();
    std::cout << "\nFinal controller state: " << motionbridge::to_string(final_status.state)
              << "\nFinal fault:            " << motionbridge::to_string(final_status.fault)
              << "\nFinal position:         " << final_status.servo.position_rad << " rad"
              << "\nPLC status heartbeat:  " << plc.last_status().controller_heartbeat
              << '\n';
    return final_status.state == motionbridge::ControllerState::ready
            && final_status.fault == motionbridge::FaultCode::none
        ? 0
        : 1;
}
