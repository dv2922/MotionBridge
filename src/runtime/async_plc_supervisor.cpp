#include "motionbridge/runtime/async_plc_supervisor.hpp"

namespace motionbridge {

AsyncPlcSupervisor::AsyncPlcSupervisor(
    double watchdog_timeout_seconds,
    PlcCommunicationWorker& communication,
    ControlLoop& control_loop)
    : communication_(communication)
    , control_loop_(control_loop)
    , watchdog_(watchdog_timeout_seconds)
{
    watchdog_.kick();
}

ControllerStatus AsyncPlcSupervisor::update(double dt_seconds) noexcept
{
    const auto command = communication_.try_read_command();
    if (command
        && (!last_command_sequence_ || command->sequence != *last_command_sequence_)) {
        control_loop_.set_command(decode_plc_command(command->value));
        last_command_sequence_ = command->sequence;
        if (!last_plc_heartbeat_ || command->value.heartbeat != *last_plc_heartbeat_) {
            last_plc_heartbeat_ = command->value.heartbeat;
            watchdog_.kick();
        }
    }

    watchdog_.advance(dt_seconds);
    communication_healthy_ =
        communication_.connected()
        && last_plc_heartbeat_.has_value()
        && !watchdog_.expired();
    control_loop_.set_external_fault(
        watchdog_.expired() ? FaultCode::communication_timeout : FaultCode::none);

    auto status = control_loop_.step(dt_seconds);
    ++controller_heartbeat_;
    (void)communication_.try_publish_status(
        encode_plc_status(status, controller_heartbeat_));
    return status;
}

bool AsyncPlcSupervisor::communication_healthy() const noexcept
{
    return communication_healthy_;
}

} // namespace motionbridge
