#pragma once

#include "motionbridge/interfaces/plc_interface.hpp"
#include "motionbridge/runtime/control_loop.hpp"
#include "motionbridge/runtime/watchdog.hpp"

#include <cstdint>
#include <optional>

namespace motionbridge {

class PlcSupervisor {
public:
    struct Configuration {
        double poll_period_seconds{0.02};
        double watchdog_timeout_seconds{0.10};
    };

    PlcSupervisor(
        Configuration configuration,
        IPlcInterface& plc,
        ControlLoop& control_loop);

    [[nodiscard]] ControllerStatus update(double dt_seconds) noexcept;
    [[nodiscard]] bool communication_healthy() const noexcept;

private:
    Configuration configuration_;
    IPlcInterface& plc_;
    ControlLoop& control_loop_;
    Watchdog watchdog_;
    double poll_elapsed_seconds_;
    std::optional<std::uint32_t> last_plc_heartbeat_;
    std::uint32_t controller_heartbeat_{0};
    bool communication_healthy_{false};
};

} // namespace motionbridge
