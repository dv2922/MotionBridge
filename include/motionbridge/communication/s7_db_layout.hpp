#pragma once

#include "motionbridge/interfaces/plc_interface.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace motionbridge::s7_db_layout {

inline constexpr int db_number = 2;
inline constexpr int command_offset = 0;
inline constexpr std::size_t command_size = 30;
inline constexpr int status_offset = 30;
inline constexpr std::size_t status_size = 32;
inline constexpr std::size_t total_size = command_size + status_size;

using CommandBuffer = std::array<std::uint8_t, command_size>;
using StatusBuffer = std::array<std::uint8_t, status_size>;

[[nodiscard]] PlcCommandData decode_command(
    std::span<const std::uint8_t, command_size> bytes) noexcept;
[[nodiscard]] StatusBuffer encode_status(const PlcStatusData& status) noexcept;

} // namespace motionbridge::s7_db_layout
