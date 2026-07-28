#pragma once

#include "motionbridge/core/types.hpp"

#include <cstdint>
#include <optional>

namespace motionbridge {

namespace plc_control_bit {
inline constexpr std::uint16_t enable = 1U << 0U;
inline constexpr std::uint16_t start = 1U << 1U;
inline constexpr std::uint16_t stop = 1U << 2U;
inline constexpr std::uint16_t reset_fault = 1U << 3U;
inline constexpr std::uint16_t emergency_stop = 1U << 4U;
inline constexpr std::uint16_t power_available = 1U << 5U;
} // namespace plc_control_bit

namespace plc_status_bit {
inline constexpr std::uint16_t ready = 1U << 0U;
inline constexpr std::uint16_t running = 1U << 1U;
inline constexpr std::uint16_t fault = 1U << 2U;
inline constexpr std::uint16_t emergency_stop = 1U << 3U;
inline constexpr std::uint16_t target_reached = 1U << 4U;
} // namespace plc_status_bit

struct PlcCommandData {
    std::uint16_t control_word{plc_control_bit::power_available};
    double target_position_rad{0.0};
    double max_velocity_rad_s{0.8};
    double max_acceleration_rad_s2{1.5};
    std::uint32_t heartbeat{0};
};

struct PlcStatusData {
    std::uint16_t status_word{0};
    double actual_position_rad{0.0};
    double actual_velocity_rad_s{0.0};
    double following_error_rad{0.0};
    std::uint16_t fault_code{0};
    std::uint32_t controller_heartbeat{0};
};

[[nodiscard]] MotionCommand decode_plc_command(const PlcCommandData& data) noexcept;
[[nodiscard]] PlcStatusData encode_plc_status(
    const ControllerStatus& status,
    std::uint32_t controller_heartbeat) noexcept;

// Future Siemens or TwinCAT adapters translate their native data model here.
class IPlcInterface {
public:
    virtual ~IPlcInterface() = default;
    virtual bool connect() = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual std::optional<PlcCommandData> read_command() = 0;
    virtual bool write_status(const PlcStatusData& status) = 0;
};

} // namespace motionbridge
