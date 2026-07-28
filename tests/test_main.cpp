#include "motionbridge/control/pid_controller.hpp"
#include "motionbridge/control/trapezoidal_trajectory.hpp"
#include "motionbridge/communication/mock_plc.hpp"
#include "motionbridge/communication/opcua_config.hpp"
#include "motionbridge/core/controller_state_machine.hpp"
#include "motionbridge/interfaces/fieldbus.hpp"
#include "motionbridge/interfaces/opcua_transport.hpp"
#include "motionbridge/interfaces/plc_interface.hpp"
#include "motionbridge/interfaces/ros_adapter.hpp"
#include "motionbridge/runtime/control_loop.hpp"
#include "motionbridge/runtime/plc_supervisor.hpp"
#include "motionbridge/runtime/watchdog.hpp"
#include "motionbridge/simulation/servo_plant.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_pid_saturation_and_anti_windup()
{
    motionbridge::PidController pid{
        {.kp = 5.0, .ki = 10.0, .kd = 0.0, .derivative_filter_time_s = 0.0},
        -2.0,
        2.0};

    for (int count = 0; count < 1000; ++count) {
        require(pid.update(10.0, 0.0, 0.001) <= 2.0, "PID exceeded upper output limit");
    }
    require(std::abs(pid.integral()) < 1e-12, "PID integral wound up during saturation");
    require(pid.update(-1.0, 0.0, 0.001) >= -2.0, "PID exceeded lower output limit");
}

void test_trajectory_respects_limits_and_reaches_target()
{
    motionbridge::TrapezoidalTrajectory trajectory;
    trajectory.reset(0.0);
    require(trajectory.set_target(2.0, 0.7, 1.2), "Valid trajectory was rejected");

    double previous_velocity = 0.0;
    motionbridge::TrajectoryPoint point = trajectory.current();
    for (int count = 0; count < 10000 && !point.finished; ++count) {
        point = trajectory.update(0.001);
        require(std::abs(point.velocity_rad_s) <= 0.700001, "Trajectory exceeded velocity limit");
        require(
            std::abs(point.velocity_rad_s - previous_velocity) <= 0.001201,
            "Trajectory exceeded acceleration limit");
        previous_velocity = point.velocity_rad_s;
    }

    require(point.finished, "Trajectory did not finish");
    require(std::abs(point.position_rad - 2.0) < 1e-9, "Trajectory missed target");
}

void test_state_machine_and_latched_fault()
{
    motionbridge::ControllerStateMachine machine;
    motionbridge::MotionCommand command;

    require(
        machine.update(command, false) == motionbridge::ControllerState::disabled,
        "Power-on transition failed");
    command.enable = true;
    require(
        machine.update(command, false) == motionbridge::ControllerState::ready,
        "Enable transition failed");
    command.start = true;
    require(
        machine.update(command, false) == motionbridge::ControllerState::running,
        "Start transition failed");

    machine.force_fault(motionbridge::FaultCode::following_error_exceeded);
    require(machine.state() == motionbridge::ControllerState::fault, "Fault state was not entered");
    command.start = false;
    machine.update(command, false);
    require(machine.state() == motionbridge::ControllerState::fault, "Fault did not latch");
    command.reset_fault = true;
    machine.update(command, false);
    require(machine.state() == motionbridge::ControllerState::disabled, "Fault reset failed");
}

void test_servo_plant_physics()
{
    motionbridge::ServoPlant plant;
    for (int count = 0; count < 100; ++count) {
        plant.update(1.0, 0.001);
    }
    require(plant.state().position_rad > 0.0, "Positive torque did not move plant");
    require(plant.state().velocity_rad_s > 0.0, "Positive torque did not accelerate plant");
    require(
        std::abs(plant.state().simulated_current_a - 2.0) < 1e-12,
        "Torque-to-current conversion is incorrect");
}

void test_control_loop_reaches_target()
{
    motionbridge::PidController pid{
        {.kp = 32.0, .ki = 5.0, .kd = 2.0, .derivative_filter_time_s = 0.008},
        -10.0,
        10.0};
    motionbridge::ControlLoop loop{
        {.frequency_hz = 1000.0,
         .position_tolerance_rad = 0.005,
         .velocity_tolerance_rad_s = 0.01},
        pid};

    motionbridge::MotionCommand command;
    loop.set_command(command);
    (void)loop.step(0.001);
    command.enable = true;
    loop.set_command(command);
    (void)loop.step(0.001);
    command.start = true;
    command.target_position_rad = 1.0;
    command.max_velocity_rad_s = 0.6;
    command.max_acceleration_rad_s2 = 1.2;
    loop.set_command(command);
    (void)loop.step(0.001);
    command.start = false;
    loop.set_command(command);

    const auto status = loop.run_for(std::chrono::duration<double>{5.0}, false);
    require(status.fault == motionbridge::FaultCode::none, "Control loop faulted");
    require(status.target_reached, "Control loop did not reach target");
    require(std::abs(status.servo.position_rad - 1.0) <= 0.005, "Final position is outside tolerance");
}

void test_plc_mapping()
{
    motionbridge::PlcCommandData data;
    data.control_word = static_cast<std::uint16_t>(
        motionbridge::plc_control_bit::power_available
        | motionbridge::plc_control_bit::enable
        | motionbridge::plc_control_bit::start);
    data.target_position_rad = 1.25;
    const auto command = motionbridge::decode_plc_command(data);
    require(command.power_available, "PLC power bit was not decoded");
    require(command.enable, "PLC enable bit was not decoded");
    require(command.start, "PLC start bit was not decoded");
    require(std::abs(command.target_position_rad - 1.25) < 1e-12, "PLC target was not decoded");

    motionbridge::ControllerStatus status;
    status.state = motionbridge::ControllerState::running;
    status.target_reached = false;
    status.servo.position_rad = 0.75;
    const auto plc_status = motionbridge::encode_plc_status(status, 42);
    require(
        (plc_status.status_word & motionbridge::plc_status_bit::running) != 0U,
        "PLC running status bit was not encoded");
    require(plc_status.controller_heartbeat == 42, "Controller heartbeat was not encoded");
}

void test_watchdog_timeout()
{
    motionbridge::Watchdog watchdog{0.05};
    watchdog.advance(1.0);
    require(!watchdog.expired(), "Unarmed watchdog expired");
    watchdog.kick();
    watchdog.advance(0.049);
    require(!watchdog.expired(), "Watchdog expired too early");
    watchdog.advance(0.002);
    require(watchdog.expired(), "Watchdog did not detect timeout");
    watchdog.kick();
    require(!watchdog.expired(), "Watchdog kick did not clear timeout");
}

void test_plc_supervisor_fault_and_recovery()
{
    motionbridge::PidController pid{
        {.kp = 32.0, .ki = 5.0, .kd = 2.0, .derivative_filter_time_s = 0.008},
        -10.0,
        10.0};
    motionbridge::ControlLoop loop{
        {.frequency_hz = 1000.0,
         .position_tolerance_rad = 0.005,
         .velocity_tolerance_rad_s = 0.01},
        pid};
    motionbridge::MockPlc plc;
    require(plc.connect(), "Mock PLC failed to connect");
    motionbridge::PlcSupervisor supervisor{
        {.poll_period_seconds = 0.005, .watchdog_timeout_seconds = 0.02},
        plc,
        loop};

    motionbridge::PlcCommandData command;
    command.heartbeat = 1;
    plc.set_command(command);
    auto status = supervisor.update(0.001);
    require(status.state == motionbridge::ControllerState::disabled, "PLC power-on failed");

    command.control_word |= motionbridge::plc_control_bit::enable;
    ++command.heartbeat;
    plc.set_command(command);
    for (int count = 0; count < 10; ++count) {
        status = supervisor.update(0.001);
    }
    require(status.state == motionbridge::ControllerState::ready, "PLC enable failed");

    command.control_word |= motionbridge::plc_control_bit::start;
    command.target_position_rad = 1.0;
    ++command.heartbeat;
    plc.set_command(command);
    for (int count = 0; count < 10; ++count) {
        status = supervisor.update(0.001);
    }
    require(status.state == motionbridge::ControllerState::running, "PLC start failed");

    command.control_word &= static_cast<std::uint16_t>(~motionbridge::plc_control_bit::start);
    ++command.heartbeat;
    plc.set_command(command);
    for (int count = 0; count < 40; ++count) {
        status = supervisor.update(0.001);
    }
    require(status.state == motionbridge::ControllerState::fault, "PLC timeout did not fault");
    require(
        status.fault == motionbridge::FaultCode::communication_timeout,
        "PLC timeout produced the wrong fault");

    command.control_word = static_cast<std::uint16_t>(
        motionbridge::plc_control_bit::power_available
        | motionbridge::plc_control_bit::reset_fault);
    ++command.heartbeat;
    plc.set_command(command);
    for (int count = 0; count < 10; ++count) {
        status = supervisor.update(0.001);
    }
    require(status.state == motionbridge::ControllerState::disabled, "PLC fault reset failed");
    require(status.fault == motionbridge::FaultCode::none, "PLC fault remained latched after reset");
}

void test_opcua_configuration()
{
    std::istringstream input{
        "endpoint=opc.tcp://127.0.0.1:4840\n"
        "namespace=3\n"
        "control_word=db.control\n"
        "target_position=db.target\n"
        "max_velocity=db.velocity\n"
        "max_acceleration=db.acceleration\n"
        "plc_heartbeat=db.plcHeartbeat\n"
        "status_word=db.status\n"
        "actual_position=db.position\n"
        "actual_velocity=db.actualVelocity\n"
        "following_error=db.error\n"
        "fault_code=db.fault\n"
        "controller_heartbeat=db.controllerHeartbeat\n"};
    const auto configuration = motionbridge::load_opcua_configuration(input);
    require(
        configuration.endpoint == "opc.tcp://127.0.0.1:4840",
        "OPC UA endpoint was not parsed");
    require(configuration.nodes.namespace_index == 3, "OPC UA namespace was not parsed");
    require(
        configuration.nodes.control_word == "db.control",
        "OPC UA control-word NodeId was not parsed");
}

} // namespace

int main()
{
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"PID saturation and anti-windup", test_pid_saturation_and_anti_windup},
        {"trajectory limits and target", test_trajectory_respects_limits_and_reaches_target},
        {"state machine and latched fault", test_state_machine_and_latched_fault},
        {"servo plant physics", test_servo_plant_physics},
        {"control-loop integration", test_control_loop_reaches_target},
        {"PLC command/status mapping", test_plc_mapping},
        {"watchdog timeout", test_watchdog_timeout},
        {"PLC supervision fault and recovery", test_plc_supervisor_fault_and_recovery},
        {"OPC UA configuration", test_opcua_configuration},
    };

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    std::cout << tests.size() - static_cast<std::size_t>(failures)
              << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
