#include "motionbridge/communication/s7_db_layout.hpp"

#include <bit>
#include <cstdint>

namespace motionbridge::s7_db_layout {
namespace {

std::uint16_t read_u16(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) << 8U | bytes[1]);
}

std::uint32_t read_u32(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0]) << 24U
        | static_cast<std::uint32_t>(bytes[1]) << 16U
        | static_cast<std::uint32_t>(bytes[2]) << 8U
        | static_cast<std::uint32_t>(bytes[3]);
}

double read_f64(const std::uint8_t* bytes) noexcept
{
    std::uint64_t bits{0};
    for (std::size_t index = 0; index < sizeof(bits); ++index) {
        bits = bits << 8U | bytes[index];
    }
    return std::bit_cast<double>(bits);
}

void write_u16(std::uint8_t* bytes, std::uint16_t value) noexcept
{
    bytes[0] = static_cast<std::uint8_t>(value >> 8U);
    bytes[1] = static_cast<std::uint8_t>(value);
}

void write_u32(std::uint8_t* bytes, std::uint32_t value) noexcept
{
    bytes[0] = static_cast<std::uint8_t>(value >> 24U);
    bytes[1] = static_cast<std::uint8_t>(value >> 16U);
    bytes[2] = static_cast<std::uint8_t>(value >> 8U);
    bytes[3] = static_cast<std::uint8_t>(value);
}

void write_f64(std::uint8_t* bytes, double value) noexcept
{
    auto bits = std::bit_cast<std::uint64_t>(value);
    for (std::size_t index = 0; index < sizeof(bits); ++index) {
        bytes[sizeof(bits) - index - 1] = static_cast<std::uint8_t>(bits);
        bits >>= 8U;
    }
}

} // namespace

PlcCommandData decode_command(
    std::span<const std::uint8_t, command_size> bytes) noexcept
{
    PlcCommandData command;
    command.control_word = read_u16(bytes.data());
    command.target_position_rad = read_f64(bytes.data() + 2);
    command.max_velocity_rad_s = read_f64(bytes.data() + 10);
    command.max_acceleration_rad_s2 = read_f64(bytes.data() + 18);
    command.heartbeat = read_u32(bytes.data() + 26);
    return command;
}

StatusBuffer encode_status(const PlcStatusData& status) noexcept
{
    StatusBuffer bytes{};
    write_u16(bytes.data(), status.status_word);
    write_f64(bytes.data() + 2, status.actual_position_rad);
    write_f64(bytes.data() + 10, status.actual_velocity_rad_s);
    write_f64(bytes.data() + 18, status.following_error_rad);
    write_u16(bytes.data() + 26, status.fault_code);
    write_u32(bytes.data() + 28, status.controller_heartbeat);
    return bytes;
}

} // namespace motionbridge::s7_db_layout
