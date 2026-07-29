#include "motionbridge/communication/s7_plc.hpp"

#include "motionbridge/communication/s7_db_layout.hpp"

#include <snap7.h>

#include <array>
#include <utility>

namespace motionbridge {
namespace {

std::string snap7_error(int code)
{
    std::array<char, 1024> text{};
    (void)Cli_ErrorText(code, text.data(), static_cast<int>(text.size()));
    return text.data();
}

} // namespace

struct S7Plc::Impl {
    explicit Impl(Configuration value)
        : configuration{std::move(value)}
        , client{Cli_Create()}
    {
    }

    Configuration configuration;
    S7Object client;
    bool connected{false};
    std::string error;
};

S7Plc::S7Plc(Configuration configuration)
    : impl_{std::make_unique<Impl>(std::move(configuration))}
{
}

S7Plc::~S7Plc()
{
    disconnect();
    Cli_Destroy(&impl_->client);
}

bool S7Plc::connect()
{
    const int result = Cli_ConnectTo(
        impl_->client,
        impl_->configuration.address.c_str(),
        impl_->configuration.rack,
        impl_->configuration.slot);
    if (result != 0) {
        impl_->connected = false;
        impl_->error = "S7 connection failed: " + snap7_error(result);
        return false;
    }
    impl_->connected = true;
    impl_->error.clear();
    return true;
}

void S7Plc::disconnect() noexcept
{
    if (impl_->connected) {
        (void)Cli_Disconnect(impl_->client);
        impl_->connected = false;
    }
}

std::optional<PlcCommandData> S7Plc::read_command()
{
    if (!impl_->connected) {
        impl_->error = "S7 client is not connected";
        return std::nullopt;
    }

    s7_db_layout::CommandBuffer bytes{};
    const int result = Cli_DBRead(
        impl_->client,
        impl_->configuration.db_number,
        s7_db_layout::command_offset,
        static_cast<int>(bytes.size()),
        bytes.data());
    if (result != 0) {
        impl_->error = "DB read failed: " + snap7_error(result);
        return std::nullopt;
    }
    impl_->error.clear();
    return s7_db_layout::decode_command(bytes);
}

bool S7Plc::write_status(const PlcStatusData& status)
{
    if (!impl_->connected) {
        impl_->error = "S7 client is not connected";
        return false;
    }

    auto bytes = s7_db_layout::encode_status(status);
    const int result = Cli_DBWrite(
        impl_->client,
        impl_->configuration.db_number,
        s7_db_layout::status_offset,
        static_cast<int>(bytes.size()),
        bytes.data());
    if (result != 0) {
        impl_->error = "DB write failed: " + snap7_error(result);
        return false;
    }
    impl_->error.clear();
    return true;
}

const std::string& S7Plc::last_error() const noexcept
{
    return impl_->error;
}

} // namespace motionbridge
