#include "motionbridge/communication/opcua_config.hpp"
#include "motionbridge/communication/opcua_plc.hpp"

#include <exception>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: motionbridge_opcua_probe <configuration-file>\n";
        return 1;
    }

    try {
        const auto configuration = motionbridge::load_opcua_configuration_file(argv[1]);
        std::cout << "Connecting to " << configuration.endpoint << " ...\n";
        motionbridge::OpcUaPlc plc{configuration};
        if (!plc.connect()) {
            std::cerr << "Connection failed: " << plc.last_error() << '\n';
            return 2;
        }
        std::cout << "OPC UA session connected.\n";

        const auto command = plc.read_command();
        if (!command) {
            std::cerr << "Command read failed: " << plc.last_error() << '\n';
            return 3;
        }

        std::cout << std::fixed << std::setprecision(3)
                  << "Control word:     0x" << std::hex << command->control_word << std::dec
                  << "\nTarget position: " << command->target_position_rad << " rad"
                  << "\nMax velocity:    " << command->max_velocity_rad_s << " rad/s"
                  << "\nMax acceleration:" << command->max_acceleration_rad_s2 << " rad/s^2"
                  << "\nPLC heartbeat:   " << command->heartbeat << '\n';
        plc.disconnect();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Configuration error: " << error.what() << '\n';
        return 1;
    }
}
