#pragma once

#include "motionbridge/interfaces/plc_interface.hpp"

#include <mutex>

namespace motionbridge {

class MockPlc final : public IPlcInterface {
public:
    bool connect() override;
    void disconnect() noexcept override;
    [[nodiscard]] std::optional<PlcCommandData> read_command() override;
    bool write_status(const PlcStatusData& status) override;

    void set_command(const PlcCommandData& command) noexcept;
    [[nodiscard]] PlcStatusData last_status() const noexcept;
    [[nodiscard]] bool connected() const noexcept;

private:
    mutable std::mutex mutex_;
    bool connected_{false};
    PlcCommandData command_{};
    PlcStatusData last_status_{};
};

} // namespace motionbridge
