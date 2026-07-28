#include "motionbridge/communication/opcua_config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace motionbridge {
namespace {

std::string trim(std::string value)
{
    const auto not_space = [](unsigned char character) {
        return !std::isspace(character);
    };
    const auto first = std::find_if(value.begin(), value.end(), not_space);
    const auto last = std::find_if(value.rbegin(), value.rend(), not_space).base();
    if (first >= last) {
        return {};
    }
    return {first, last};
}

const std::string& required(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view key)
{
    const auto found = values.find(std::string{key});
    if (found == values.end() || found->second.empty()) {
        throw std::runtime_error("Missing OPC UA configuration key: " + std::string{key});
    }
    return found->second;
}

} // namespace

OpcUaConfiguration load_opcua_configuration(std::istream& input)
{
    std::unordered_map<std::string, std::string> values;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(std::move(line));
        if (line.empty()) {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::runtime_error(
                "Invalid OPC UA configuration line " + std::to_string(line_number));
        }
        auto key = trim(line.substr(0, separator));
        auto value = trim(line.substr(separator + 1));
        if (key.empty() || value.empty()) {
            throw std::runtime_error(
                "Invalid OPC UA configuration line " + std::to_string(line_number));
        }
        values[std::move(key)] = std::move(value);
    }

    OpcUaConfiguration configuration;
    configuration.endpoint = required(values, "endpoint");
    const auto namespace_value = std::stoul(required(values, "namespace"));
    if (namespace_value > 65535U) {
        throw std::runtime_error("OPC UA namespace must fit in UInt16");
    }
    configuration.nodes.namespace_index = static_cast<std::uint16_t>(namespace_value);
    configuration.nodes.control_word = required(values, "control_word");
    configuration.nodes.target_position = required(values, "target_position");
    configuration.nodes.max_velocity = required(values, "max_velocity");
    configuration.nodes.max_acceleration = required(values, "max_acceleration");
    configuration.nodes.plc_heartbeat = required(values, "plc_heartbeat");
    configuration.nodes.status_word = required(values, "status_word");
    configuration.nodes.actual_position = required(values, "actual_position");
    configuration.nodes.actual_velocity = required(values, "actual_velocity");
    configuration.nodes.following_error = required(values, "following_error");
    configuration.nodes.fault_code = required(values, "fault_code");
    configuration.nodes.controller_heartbeat = required(values, "controller_heartbeat");
    return configuration;
}

OpcUaConfiguration load_opcua_configuration_file(const std::string& path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Unable to open OPC UA configuration: " + path);
    }
    return load_opcua_configuration(input);
}

} // namespace motionbridge
