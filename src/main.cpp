#include "motionbridge/runtime/control_loop.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Options {
    double target{1.57};
    double max_velocity{0.8};
    double max_acceleration{1.5};
    double duration{5.0};
    double frequency{1000.0};
    std::string csv_file{};
    bool fast{false};
};

void print_help()
{
    std::cout
        << "MotionBridge Milestone 1 demo\n\n"
        << "Options:\n"
        << "  --target <rad>               Target position (default: 1.57)\n"
        << "  --max-velocity <rad/s>       Velocity limit (default: 0.8)\n"
        << "  --max-acceleration <rad/s2>  Acceleration limit (default: 1.5)\n"
        << "  --duration <seconds>         Maximum duration (default: 5.0)\n"
        << "  --frequency <Hz>             Loop frequency (default: 1000)\n"
        << "  --csv <path>                 Save cycle telemetry as CSV\n"
        << "  --fast                       Disable real-time sleeping\n"
        << "  --help                       Show this help\n";
}

double parse_number(const char* text, std::string_view option)
{
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (end == text || *end != '\0' || !std::isfinite(value)) {
        throw std::invalid_argument("Invalid number for " + std::string{option});
    }
    return value;
}

Options parse_options(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help") {
            print_help();
            std::exit(0);
        }
        if (argument == "--fast") {
            options.fast = true;
            continue;
        }
        if (argument == "--csv") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("Missing value for --csv");
            }
            options.csv_file = argv[++index];
            continue;
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument("Missing value for " + std::string{argument});
        }
        const double value = parse_number(argv[++index], argument);
        if (argument == "--target") {
            options.target = value;
        } else if (argument == "--max-velocity") {
            options.max_velocity = value;
        } else if (argument == "--max-acceleration") {
            options.max_acceleration = value;
        } else if (argument == "--duration") {
            options.duration = value;
        } else if (argument == "--frequency") {
            options.frequency = value;
        } else {
            throw std::invalid_argument("Unknown option: " + std::string{argument});
        }
    }
    if (options.max_velocity <= 0.0
        || options.max_acceleration <= 0.0
        || options.duration <= 0.0
        || options.frequency <= 0.0) {
        throw std::invalid_argument("Velocity, acceleration, duration, and frequency must be positive");
    }
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parse_options(argc, argv);
        motionbridge::PidController controller{
            {.kp = 32.0, .ki = 5.0, .kd = 2.0, .derivative_filter_time_s = 0.008},
            -10.0,
            10.0};
        motionbridge::ControlLoop loop{
            {.frequency_hz = options.frequency,
             .position_tolerance_rad = 0.005,
             .velocity_tolerance_rad_s = 0.01},
            controller};

        const double dt = 1.0 / options.frequency;
        motionbridge::MotionCommand command;
        loop.set_command(command);
        (void)loop.step(dt); // POWER_OFF -> DISABLED

        command.enable = true;
        loop.set_command(command);
        (void)loop.step(dt); // DISABLED -> READY

        command.start = true;
        command.target_position_rad = options.target;
        command.max_velocity_rad_s = options.max_velocity;
        command.max_acceleration_rad_s2 = options.max_acceleration;
        loop.set_command(command);
        (void)loop.step(dt); // READY -> RUNNING

        command.start = false;
        loop.set_command(command);

        std::ofstream csv;
        if (!options.csv_file.empty()) {
            const std::filesystem::path csv_path{options.csv_file};
            if (csv_path.has_parent_path()) {
                std::filesystem::create_directories(csv_path.parent_path());
            }
            csv.open(csv_path);
            if (!csv) {
                throw std::runtime_error("Unable to open CSV output: " + options.csv_file);
            }
            csv << "time_s,state,reference_position_rad,actual_position_rad,"
                   "reference_velocity_rad_s,actual_velocity_rad_s,torque_nm,"
                   "following_error_rad\n";
            csv << std::fixed << std::setprecision(9);
        }

        auto last_report_cycle = std::uint64_t{0};
        const auto status = loop.run_for(
            std::chrono::duration<double>{options.duration},
            !options.fast,
            [&](const motionbridge::ControllerStatus& current) {
                if (csv) {
                    const double time_s =
                        static_cast<double>(current.timing.cycles) / options.frequency;
                    csv << time_s << ','
                        << motionbridge::to_string(current.state) << ','
                        << current.reference.position_rad << ','
                        << current.servo.position_rad << ','
                        << current.reference.velocity_rad_s << ','
                        << current.servo.velocity_rad_s << ','
                        << current.servo.commanded_torque_nm << ','
                        << current.following_error_rad << '\n';
                }
                const auto interval = static_cast<std::uint64_t>(options.frequency / 4.0);
                if (interval > 0 && current.timing.cycles >= last_report_cycle + interval) {
                    last_report_cycle = current.timing.cycles;
                    std::cout
                        << std::fixed << std::setprecision(3)
                        << "cycle=" << current.timing.cycles
                        << " state=" << motionbridge::to_string(current.state)
                        << " ref=" << current.reference.position_rad
                        << " pos=" << current.servo.position_rad
                        << " vel=" << current.servo.velocity_rad_s
                        << " torque=" << current.servo.commanded_torque_nm
                        << '\n';
                }
            });

        std::cout
            << "\nFinal state:          " << motionbridge::to_string(status.state) << '\n'
            << "Fault:                " << motionbridge::to_string(status.fault) << '\n'
            << "Target position:      " << options.target << " rad\n"
            << "Actual position:      " << status.servo.position_rad << " rad\n"
            << "Following error:      " << status.following_error_rad << " rad\n"
            << "Control cycles:       " << status.timing.cycles << '\n'
            << "Mean execution time:  " << status.timing.mean_execution_us << " us\n"
            << "Maximum execution:    " << status.timing.maximum_execution_us << " us\n"
            << "Maximum start jitter: " << status.timing.maximum_jitter_us << " us\n"
            << "Deadline misses:      " << status.timing.deadline_misses << '\n';

        if (csv) {
            std::cout << "Telemetry CSV:        " << options.csv_file << '\n';
        }

        return status.target_reached && status.fault == motionbridge::FaultCode::none ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
