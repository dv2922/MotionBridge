#include "motionbridge/interfaces/plc_interface.hpp"

namespace motionbridge {

MotionCommand decode_plc_command(const PlcCommandData& data) noexcept
{
    const auto bit_is_set = [&data](std::uint16_t bit) {
        return (data.control_word & bit) != 0U;
    };

    return {
        .power_available = bit_is_set(plc_control_bit::power_available),
        .enable = bit_is_set(plc_control_bit::enable),
        .start = bit_is_set(plc_control_bit::start),
        .stop = bit_is_set(plc_control_bit::stop),
        .reset_fault = bit_is_set(plc_control_bit::reset_fault),
        .emergency_stop = bit_is_set(plc_control_bit::emergency_stop),
        .target_position_rad = data.target_position_rad,
        .max_velocity_rad_s = data.max_velocity_rad_s,
        .max_acceleration_rad_s2 = data.max_acceleration_rad_s2,
    };
}

PlcStatusData encode_plc_status(
    const ControllerStatus& status,
    std::uint32_t controller_heartbeat) noexcept
{
    std::uint16_t status_word = 0U;
    if (status.state == ControllerState::ready) {
        status_word |= plc_status_bit::ready;
    }
    if (status.state == ControllerState::running) {
        status_word |= plc_status_bit::running;
    }
    if (status.state == ControllerState::fault) {
        status_word |= plc_status_bit::fault;
    }
    if (status.state == ControllerState::emergency_stop) {
        status_word |= plc_status_bit::emergency_stop;
    }
    if (status.target_reached) {
        status_word |= plc_status_bit::target_reached;
    }

    return {
        .status_word = status_word,
        .actual_position_rad = status.servo.position_rad,
        .actual_velocity_rad_s = status.servo.velocity_rad_s,
        .following_error_rad = status.following_error_rad,
        .fault_code = static_cast<std::uint16_t>(status.fault),
        .controller_heartbeat = controller_heartbeat,
    };
}

} // namespace motionbridge
