#include "motionbridge/communication/mock_plc.hpp"
#include "motionbridge/communication/plc_communication_worker.hpp"
#include "motionbridge/runtime/async_plc_supervisor.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

int main()
{
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    constexpr double dt = 0.001;
    constexpr std::uint64_t total_cycles = 1500;

    motionbridge::MockPlc plc;
    motionbridge::PlcCommandData command;
    command.target_position_rad = 2.0;
    command.max_velocity_rad_s = 0.8;
    command.max_acceleration_rad_s2 = 1.5;
    command.heartbeat = 1;
    plc.set_command(command);

    motionbridge::PlcCommunicationWorker communication{
        {.poll_period = 20ms, .reconnect_period = 100ms},
        plc};
    motionbridge::PidController pid{
        {.kp = 32.0, .ki = 5.0, .kd = 2.0, .derivative_filter_time_s = 0.008},
        -10.0,
        10.0};
    motionbridge::ControlLoop control_loop{
        {.frequency_hz = 1000.0,
         .position_tolerance_rad = 0.005,
         .velocity_tolerance_rad_s = 0.01},
        pid};
    motionbridge::AsyncPlcSupervisor supervisor{0.10, communication, control_loop};
    communication.start();

    auto next_cycle = Clock::now();
    auto previous_state = motionbridge::ControllerState::power_off;
    for (std::uint64_t cycle = 0; cycle < total_cycles; ++cycle) {
        const double time_s = static_cast<double>(cycle) * dt;
        if (cycle == 40) {
            command.control_word |= motionbridge::plc_control_bit::enable;
            ++command.heartbeat;
            plc.set_command(command);
            std::cout << "[0.040 s] PLC thread: enable axis\n";
        } else if (cycle == 80) {
            command.control_word |= motionbridge::plc_control_bit::start;
            ++command.heartbeat;
            plc.set_command(command);
            std::cout << "[0.080 s] PLC thread: start move\n";
        } else if (cycle == 140) {
            command.control_word &= static_cast<std::uint16_t>(
                ~motionbridge::plc_control_bit::start);
            ++command.heartbeat;
            plc.set_command(command);
        } else if (cycle > 140 && cycle < 900 && cycle % 20 == 0) {
            ++command.heartbeat;
            plc.set_command(command);
        } else if (cycle == 900) {
            std::cout << "[0.900 s] PLC thread: heartbeat frozen\n";
        } else if (cycle == 1200) {
            command.control_word = static_cast<std::uint16_t>(
                motionbridge::plc_control_bit::power_available
                | motionbridge::plc_control_bit::reset_fault);
            ++command.heartbeat;
            plc.set_command(command);
            std::cout << "[1.200 s] PLC thread: heartbeat restored + reset\n";
        } else if (cycle == 1260) {
            command.control_word = static_cast<std::uint16_t>(
                motionbridge::plc_control_bit::power_available
                | motionbridge::plc_control_bit::enable);
            ++command.heartbeat;
            plc.set_command(command);
        } else if (cycle > 1200 && cycle % 20 == 0) {
            ++command.heartbeat;
            plc.set_command(command);
        }

        const auto status = supervisor.update(dt);
        if (status.state != previous_state) {
            std::cout << std::fixed << std::setprecision(3)
                      << '[' << time_s << " s] 1 kHz loop: "
                      << motionbridge::to_string(previous_state) << " -> "
                      << motionbridge::to_string(status.state);
            if (status.fault != motionbridge::FaultCode::none) {
                std::cout << " (" << motionbridge::to_string(status.fault) << ')';
            }
            std::cout << '\n';
            previous_state = status.state;
        }

        next_cycle += 1ms;
        std::this_thread::sleep_until(next_cycle);
    }

    communication.stop();
    const auto statistics = communication.statistics();
    const auto status = control_loop.status();
    std::cout << "\nFinal state:        " << motionbridge::to_string(status.state)
              << "\nPLC reads (50 Hz):  " << statistics.successful_reads
              << "\nPLC writes (50 Hz): " << statistics.successful_writes
              << "\nControl cycles:     " << total_cycles << '\n';
    return status.state == motionbridge::ControllerState::ready ? 0 : 1;
}
