#include "motionbridge/control/pid_controller.hpp"
#include "motionbridge/control/trapezoidal_trajectory.hpp"
#include "motionbridge/core/controller_state_machine.hpp"
#include "motionbridge/interfaces/fieldbus.hpp"
#include "motionbridge/interfaces/opcua_transport.hpp"
#include "motionbridge/interfaces/plc_interface.hpp"
#include "motionbridge/interfaces/ros_adapter.hpp"
#include "motionbridge/runtime/control_loop.hpp"
#include "motionbridge/simulation/servo_plant.hpp"

#include <cmath>
#include <functional>
#include <iostream>
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

} // namespace

int main()
{
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"PID saturation and anti-windup", test_pid_saturation_and_anti_windup},
        {"trajectory limits and target", test_trajectory_respects_limits_and_reaches_target},
        {"state machine and latched fault", test_state_machine_and_latched_fault},
        {"servo plant physics", test_servo_plant_physics},
        {"control-loop integration", test_control_loop_reaches_target},
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
