#include "motionbridge/communication/opcua_plc.hpp"

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>

#include <cstdint>
#include <string>
#include <utility>

namespace motionbridge {
namespace {

template <typename Value>
bool read_scalar(
    UA_Client* client,
    std::uint16_t namespace_index,
    const std::string& identifier,
    const UA_DataType& type,
    Value& output)
{
    UA_NodeId node = UA_NODEID_STRING_ALLOC(namespace_index, identifier.c_str());
    UA_Variant value;
    UA_Variant_init(&value);
    const UA_StatusCode result = UA_Client_readValueAttribute(client, node, &value);
    const bool valid =
        result == UA_STATUSCODE_GOOD
        && UA_Variant_hasScalarType(&value, &type)
        && value.data != nullptr;
    if (valid) {
        output = *static_cast<Value*>(value.data);
    }
    UA_Variant_clear(&value);
    UA_NodeId_clear(&node);
    return valid;
}

template <typename Value>
bool write_scalar(
    UA_Client* client,
    std::uint16_t namespace_index,
    const std::string& identifier,
    const UA_DataType& type,
    const Value& input)
{
    UA_NodeId node = UA_NODEID_STRING_ALLOC(namespace_index, identifier.c_str());
    UA_Variant value;
    UA_Variant_init(&value);
    const UA_StatusCode copy_result = UA_Variant_setScalarCopy(&value, &input, &type);
    const UA_StatusCode write_result = copy_result == UA_STATUSCODE_GOOD
        ? UA_Client_writeValueAttribute(client, node, &value)
        : copy_result;
    UA_Variant_clear(&value);
    UA_NodeId_clear(&node);
    return write_result == UA_STATUSCODE_GOOD;
}

} // namespace

class OpcUaPlc::Impl {
public:
    explicit Impl(OpcUaConfiguration input)
        : configuration(std::move(input))
    {
    }

    ~Impl()
    {
        close();
    }

    void close() noexcept
    {
        if (client != nullptr) {
            (void)UA_Client_disconnect(client);
            UA_Client_delete(client);
            client = nullptr;
        }
    }

    OpcUaConfiguration configuration;
    UA_Client* client{nullptr};
    std::string error;
};

OpcUaPlc::OpcUaPlc(OpcUaConfiguration configuration)
    : impl_(std::make_unique<Impl>(std::move(configuration)))
{
}

OpcUaPlc::~OpcUaPlc() = default;
OpcUaPlc::OpcUaPlc(OpcUaPlc&&) noexcept = default;
OpcUaPlc& OpcUaPlc::operator=(OpcUaPlc&&) noexcept = default;

bool OpcUaPlc::connect()
{
    impl_->close();
    impl_->client = UA_Client_new();
    if (impl_->client == nullptr) {
        impl_->error = "UA_Client_new failed";
        return false;
    }
    UA_ClientConfig_setDefault(UA_Client_getConfig(impl_->client));
    const UA_StatusCode result =
        UA_Client_connect(impl_->client, impl_->configuration.endpoint.c_str());
    if (result != UA_STATUSCODE_GOOD) {
        impl_->error = UA_StatusCode_name(result);
        impl_->close();
        return false;
    }
    impl_->error.clear();
    return true;
}

void OpcUaPlc::disconnect() noexcept
{
    impl_->close();
}

std::optional<PlcCommandData> OpcUaPlc::read_command()
{
    if (impl_->client == nullptr) {
        impl_->error = "OPC UA client is not connected";
        return std::nullopt;
    }

    const auto& nodes = impl_->configuration.nodes;
    PlcCommandData command;
    const bool success =
        read_scalar(impl_->client, nodes.namespace_index, nodes.control_word,
            UA_TYPES[UA_TYPES_UINT16], command.control_word)
        && read_scalar(impl_->client, nodes.namespace_index, nodes.target_position,
            UA_TYPES[UA_TYPES_DOUBLE], command.target_position_rad)
        && read_scalar(impl_->client, nodes.namespace_index, nodes.max_velocity,
            UA_TYPES[UA_TYPES_DOUBLE], command.max_velocity_rad_s)
        && read_scalar(impl_->client, nodes.namespace_index, nodes.max_acceleration,
            UA_TYPES[UA_TYPES_DOUBLE], command.max_acceleration_rad_s2)
        && read_scalar(impl_->client, nodes.namespace_index, nodes.plc_heartbeat,
            UA_TYPES[UA_TYPES_UINT32], command.heartbeat);
    if (!success) {
        impl_->error = "Failed to read one or more command nodes; verify NodeIds and PLC types";
        return std::nullopt;
    }
    impl_->error.clear();
    return command;
}

bool OpcUaPlc::write_status(const PlcStatusData& status)
{
    if (impl_->client == nullptr) {
        impl_->error = "OPC UA client is not connected";
        return false;
    }

    const auto& nodes = impl_->configuration.nodes;
    const bool success =
        write_scalar(impl_->client, nodes.namespace_index, nodes.status_word,
            UA_TYPES[UA_TYPES_UINT16], status.status_word)
        && write_scalar(impl_->client, nodes.namespace_index, nodes.actual_position,
            UA_TYPES[UA_TYPES_DOUBLE], status.actual_position_rad)
        && write_scalar(impl_->client, nodes.namespace_index, nodes.actual_velocity,
            UA_TYPES[UA_TYPES_DOUBLE], status.actual_velocity_rad_s)
        && write_scalar(impl_->client, nodes.namespace_index, nodes.following_error,
            UA_TYPES[UA_TYPES_DOUBLE], status.following_error_rad)
        && write_scalar(impl_->client, nodes.namespace_index, nodes.fault_code,
            UA_TYPES[UA_TYPES_UINT16], status.fault_code)
        && write_scalar(impl_->client, nodes.namespace_index, nodes.controller_heartbeat,
            UA_TYPES[UA_TYPES_UINT32], status.controller_heartbeat);
    if (!success) {
        impl_->error = "Failed to write one or more status nodes; verify NodeIds and access rights";
        return false;
    }
    impl_->error.clear();
    return true;
}

const std::string& OpcUaPlc::last_error() const noexcept
{
    return impl_->error;
}

} // namespace motionbridge
