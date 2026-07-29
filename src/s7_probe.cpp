#include "motionbridge/communication/s7_plc.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    motionbridge::S7Plc::Configuration configuration;
    if (argc >= 2) {
        configuration.address = argv[1];
    }
    if (argc >= 3) {
        configuration.db_number = std::stoi(argv[2]);
    }
    if (argc > 3) {
        std::cerr << "Usage: motionbridge_s7_probe [PLC-address] [DB-number]\n";
        return EXIT_FAILURE;
    }

    std::cout << "Connecting to S7 PLC at " << configuration.address
              << " (rack " << configuration.rack
              << ", slot " << configuration.slot
              << ", DB" << configuration.db_number << ") ...\n";

    motionbridge::S7Plc plc{configuration};
    if (!plc.connect()) {
        std::cerr << plc.last_error() << '\n';
        return 2;
    }
    std::cout << "S7 session connected.\n";

    const auto command = plc.read_command();
    if (!command) {
        std::cerr << plc.last_error() << '\n';
        return 3;
    }

    std::cout << std::fixed << std::setprecision(3)
              << "Control word:      0x" << std::hex << command->control_word << std::dec
              << "\nTarget position:  " << command->target_position_rad << " rad"
              << "\nMax velocity:     " << command->max_velocity_rad_s << " rad/s"
              << "\nMax acceleration: " << command->max_acceleration_rad_s2 << " rad/s^2"
              << "\nPLC heartbeat:    " << command->heartbeat << '\n';
    plc.disconnect();
    return EXIT_SUCCESS;
}
