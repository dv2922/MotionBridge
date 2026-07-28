#pragma once

#include <cstddef>
#include <cstdint>

namespace motionbridge {

struct DriveFeedback {
    std::uint16_t status_word{0};
    std::int32_t actual_position_counts{0};
    std::int32_t actual_velocity_counts_s{0};
    std::int16_t actual_torque_per_mille{0};
};

struct DriveCommand {
    std::uint16_t control_word{0};
    std::int8_t operating_mode{0};
    std::int32_t target_position_counts{0};
    std::int32_t target_velocity_counts_s{0};
    std::int16_t target_torque_per_mille{0};
};

// A fake PDO backend, IgH EtherCAT, or TwinCAT ADS adapter can implement this.
class IFieldbus {
public:
    virtual ~IFieldbus() = default;
    virtual bool configure() = 0;
    virtual bool activate() = 0;
    virtual void deactivate() noexcept = 0;
    virtual bool receive() noexcept = 0;
    virtual bool send() noexcept = 0;
    [[nodiscard]] virtual DriveFeedback feedback(std::size_t axis) const noexcept = 0;
    virtual void command(std::size_t axis, const DriveCommand& command) noexcept = 0;
};

} // namespace motionbridge

