#pragma once

#include "motionbridge/interfaces/plc_interface.hpp"

#include <memory>
#include <string>

namespace motionbridge {

class S7Plc final : public IPlcInterface {
public:
    struct Configuration {
        std::string address{"192.168.10.1"};
        int rack{0};
        int slot{1};
        int db_number{2};
    };

    explicit S7Plc(Configuration configuration);
    ~S7Plc() override;

    S7Plc(const S7Plc&) = delete;
    S7Plc& operator=(const S7Plc&) = delete;

    bool connect() override;
    void disconnect() noexcept override;
    [[nodiscard]] std::optional<PlcCommandData> read_command() override;
    bool write_status(const PlcStatusData& status) override;

    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace motionbridge
