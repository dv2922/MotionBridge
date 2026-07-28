#include "motionbridge/communication/mock_plc.hpp"

namespace motionbridge {

bool MockPlc::connect()
{
    const std::scoped_lock lock{mutex_};
    connected_ = true;
    return true;
}

void MockPlc::disconnect() noexcept
{
    const std::scoped_lock lock{mutex_};
    connected_ = false;
}

std::optional<PlcCommandData> MockPlc::read_command()
{
    const std::scoped_lock lock{mutex_};
    if (!connected_) {
        return std::nullopt;
    }
    return command_;
}

bool MockPlc::write_status(const PlcStatusData& status)
{
    const std::scoped_lock lock{mutex_};
    if (!connected_) {
        return false;
    }
    last_status_ = status;
    return true;
}

void MockPlc::set_command(const PlcCommandData& command) noexcept
{
    const std::scoped_lock lock{mutex_};
    command_ = command;
}

PlcStatusData MockPlc::last_status() const noexcept
{
    const std::scoped_lock lock{mutex_};
    return last_status_;
}

bool MockPlc::connected() const noexcept
{
    const std::scoped_lock lock{mutex_};
    return connected_;
}

} // namespace motionbridge
