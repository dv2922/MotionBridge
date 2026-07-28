#pragma once

#include "motionbridge/communication/opcua_config.hpp"
#include "motionbridge/interfaces/plc_interface.hpp"

#include <memory>

namespace motionbridge {

class OpcUaPlc final : public IPlcInterface {
public:
    explicit OpcUaPlc(OpcUaConfiguration configuration);
    ~OpcUaPlc() override;

    OpcUaPlc(const OpcUaPlc&) = delete;
    OpcUaPlc& operator=(const OpcUaPlc&) = delete;
    OpcUaPlc(OpcUaPlc&&) noexcept;
    OpcUaPlc& operator=(OpcUaPlc&&) noexcept;

    bool connect() override;
    void disconnect() noexcept override;
    [[nodiscard]] std::optional<PlcCommandData> read_command() override;
    bool write_status(const PlcStatusData& status) override;

    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace motionbridge
