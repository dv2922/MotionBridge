#include "motionbridge/communication/mock_plc.hpp"

namespace motionbridge {

bool MockPlc::connect()
{
    connected_ = true;
    return true;
}

void MockPlc::disconnect() noexcept
{
    connected_ = false;
}

std::optional<PlcCommandData> MockPlc::read_command()
{
    if (!connected_) {
        return std::nullopt;
    }
    return command_;
}

bool MockPlc::write_status(const PlcStatusData& status)
{
    if (!connected_) {
        return false;
    }
    last_status_ = status;
    return true;
}

void MockPlc::set_command(const PlcCommandData& command) noexcept
{
    command_ = command;
}

const PlcStatusData& MockPlc::last_status() const noexcept
{
    return last_status_;
}

bool MockPlc::connected() const noexcept
{
    return connected_;
}

} // namespace motionbridge
