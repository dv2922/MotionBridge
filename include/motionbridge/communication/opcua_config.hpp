#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace motionbridge {

struct OpcUaNodeMap {
    std::uint16_t namespace_index{3};
    std::string control_word;
    std::string target_position;
    std::string max_velocity;
    std::string max_acceleration;
    std::string plc_heartbeat;
    std::string status_word;
    std::string actual_position;
    std::string actual_velocity;
    std::string following_error;
    std::string fault_code;
    std::string controller_heartbeat;
};

struct OpcUaConfiguration {
    std::string endpoint;
    OpcUaNodeMap nodes;
};

[[nodiscard]] OpcUaConfiguration load_opcua_configuration(std::istream& input);
[[nodiscard]] OpcUaConfiguration load_opcua_configuration_file(const std::string& path);

} // namespace motionbridge
