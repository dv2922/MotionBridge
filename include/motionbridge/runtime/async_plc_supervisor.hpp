#pragma once

#include "motionbridge/communication/plc_communication_worker.hpp"
#include "motionbridge/runtime/control_loop.hpp"
#include "motionbridge/runtime/watchdog.hpp"

#include <cstdint>
#include <optional>

namespace motionbridge {

class AsyncPlcSupervisor {
public:
    AsyncPlcSupervisor(
        double watchdog_timeout_seconds,
        PlcCommunicationWorker& communication,
        ControlLoop& control_loop);

    [[nodiscard]] ControllerStatus update(double dt_seconds) noexcept;
    [[nodiscard]] bool communication_healthy() const noexcept;

private:
    PlcCommunicationWorker& communication_;
    ControlLoop& control_loop_;
    Watchdog watchdog_;
    std::optional<std::uint64_t> last_command_sequence_;
    std::optional<std::uint32_t> last_plc_heartbeat_;
    std::uint32_t controller_heartbeat_{0};
    bool communication_healthy_{false};
};

} // namespace motionbridge
