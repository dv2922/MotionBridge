#include "motionbridge/communication/plc_communication_worker.hpp"
#include "motionbridge/communication/s7_plc.hpp"
#include "motionbridge/runtime/async_plc_supervisor.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv)
{
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    motionbridge::S7Plc::Configuration plc_configuration;
    double duration_seconds = 20.0;
    if (argc >= 2) {
        plc_configuration.address = argv[1];
    }
    if (argc >= 3) {
        plc_configuration.db_number = std::stoi(argv[2]);
    }
    if (argc >= 4) {
        duration_seconds = std::stod(argv[3]);
    }
    if (argc > 4 || !(duration_seconds > 0.0)) {
        std::cerr << "Usage: motionbridge_s7_live_demo "
                     "[PLC-address] [DB-number] [duration-seconds]\n";
        return EXIT_FAILURE;
    }

    constexpr double dt = 0.001;
    const auto total_cycles =
        static_cast<std::uint64_t>(duration_seconds / dt);

    motionbridge::S7Plc plc{plc_configuration};
    motionbridge::PlcCommunicationWorker communication{
        {.poll_period = 20ms, .reconnect_period = 250ms},
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
    motionbridge::AsyncPlcSupervisor supervisor{0.25, communication, control_loop};

    std::cout << "MotionBridge live Siemens S7 demo\n"
              << "PLC: " << plc_configuration.address
              << "  DB: " << plc_configuration.db_number
              << "  control: 1000 Hz  communication: 50 Hz\n"
              << "Waiting for PLC commands. Press Ctrl+C to stop.\n\n";

    communication.start();
    auto next_cycle = Clock::now();
    auto previous_state = motionbridge::ControllerState::power_off;

    for (std::uint64_t cycle = 0; cycle < total_cycles; ++cycle) {
        const auto status = supervisor.update(dt);
        if (status.state != previous_state || cycle % 250 == 0) {
            std::cout << std::fixed << std::setprecision(3)
                      << "t=" << static_cast<double>(cycle) * dt
                      << " state=" << motionbridge::to_string(status.state)
                      << " ref=" << status.reference.position_rad
                      << " pos=" << status.servo.position_rad
                      << " vel=" << status.servo.velocity_rad_s
                      << " error=" << status.following_error_rad;
            if (status.fault != motionbridge::FaultCode::none) {
                std::cout << " fault=" << motionbridge::to_string(status.fault);
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
    std::cout << "\nFinal state:         " << motionbridge::to_string(status.state)
              << "\nFinal position:      " << status.servo.position_rad << " rad"
              << "\nFollowing error:     " << status.following_error_rad << " rad"
              << "\nPLC reads:           " << statistics.successful_reads
              << "\nPLC writes:          " << statistics.successful_writes
              << "\nCommunication errors:" << statistics.communication_errors
              << '\n';
    return statistics.successful_reads > 0 ? EXIT_SUCCESS : 2;
}
