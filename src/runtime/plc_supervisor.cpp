#include "motionbridge/runtime/plc_supervisor.hpp"

#include <cmath>
#include <stdexcept>

namespace motionbridge {

PlcSupervisor::PlcSupervisor(
    Configuration configuration,
    IPlcInterface& plc,
    ControlLoop& control_loop)
    : configuration_(configuration)
    , plc_(plc)
    , control_loop_(control_loop)
    , watchdog_(configuration.watchdog_timeout_seconds)
    , poll_elapsed_seconds_(configuration.poll_period_seconds)
{
    if (!(configuration_.poll_period_seconds > 0.0)
        || !std::isfinite(configuration_.poll_period_seconds)) {
        throw std::invalid_argument("PLC poll period must be positive and finite");
    }
}

ControllerStatus PlcSupervisor::update(double dt_seconds) noexcept
{
    poll_elapsed_seconds_ += dt_seconds;
    bool polled = false;

    if (poll_elapsed_seconds_ >= configuration_.poll_period_seconds) {
        poll_elapsed_seconds_ = 0.0;
        polled = true;
        const auto plc_command = plc_.read_command();
        if (plc_command) {
            control_loop_.set_command(decode_plc_command(*plc_command));
            if (!last_plc_heartbeat_ || *last_plc_heartbeat_ != plc_command->heartbeat) {
                watchdog_.kick();
                last_plc_heartbeat_ = plc_command->heartbeat;
            }
        }
    }

    watchdog_.advance(dt_seconds);
    communication_healthy_ = !watchdog_.expired() && last_plc_heartbeat_.has_value();
    control_loop_.set_external_fault(
        communication_healthy_ ? FaultCode::none : FaultCode::communication_timeout);
    auto status = control_loop_.step(dt_seconds);

    if (polled) {
        ++controller_heartbeat_;
        (void)plc_.write_status(encode_plc_status(status, controller_heartbeat_));
    }
    return status;
}

bool PlcSupervisor::communication_healthy() const noexcept
{
    return communication_healthy_;
}

} // namespace motionbridge
