#pragma once

#include "motionbridge/core/types.hpp"

namespace motionbridge {

class ControllerStateMachine {
public:
    [[nodiscard]] ControllerState state() const noexcept;
    [[nodiscard]] FaultCode fault() const noexcept;

    ControllerState update(
        const MotionCommand& command,
        bool target_reached,
        FaultCode detected_fault = FaultCode::none) noexcept;

    void force_fault(FaultCode fault) noexcept;

private:
    ControllerState state_{ControllerState::power_off};
    FaultCode fault_{FaultCode::none};
};

} // namespace motionbridge

