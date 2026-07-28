#include "motionbridge/core/controller_state_machine.hpp"

namespace motionbridge {

ControllerState ControllerStateMachine::state() const noexcept
{
    return state_;
}

FaultCode ControllerStateMachine::fault() const noexcept
{
    return fault_;
}

void ControllerStateMachine::force_fault(FaultCode fault) noexcept
{
    if (fault != FaultCode::none) {
        fault_ = fault;
        state_ = ControllerState::fault;
    }
}

ControllerState ControllerStateMachine::update(
    const MotionCommand& command,
    bool target_reached,
    FaultCode detected_fault) noexcept
{
    if (!command.power_available) {
        state_ = ControllerState::power_off;
        fault_ = FaultCode::none;
        return state_;
    }

    if (command.emergency_stop) {
        state_ = ControllerState::emergency_stop;
        return state_;
    }

    if (detected_fault != FaultCode::none) {
        force_fault(detected_fault);
        return state_;
    }

    if (state_ == ControllerState::emergency_stop) {
        if (command.reset_fault) {
            state_ = ControllerState::disabled;
            fault_ = FaultCode::none;
        }
        return state_;
    }

    if (state_ == ControllerState::fault) {
        if (command.reset_fault) {
            state_ = ControllerState::disabled;
            fault_ = FaultCode::none;
        }
        return state_;
    }

    switch (state_) {
    case ControllerState::power_off:
        state_ = ControllerState::disabled;
        break;
    case ControllerState::disabled:
        if (command.enable) {
            state_ = ControllerState::ready;
        }
        break;
    case ControllerState::ready:
        if (!command.enable) {
            state_ = ControllerState::disabled;
        } else if (command.start) {
            state_ = ControllerState::running;
        }
        break;
    case ControllerState::running:
        if (!command.enable) {
            state_ = ControllerState::disabled;
        } else if (command.stop) {
            state_ = ControllerState::stopping;
        } else if (target_reached) {
            state_ = ControllerState::ready;
        }
        break;
    case ControllerState::stopping:
        state_ = command.enable ? ControllerState::ready : ControllerState::disabled;
        break;
    case ControllerState::fault:
    case ControllerState::emergency_stop:
        break;
    }

    return state_;
}

} // namespace motionbridge

