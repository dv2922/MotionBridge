#pragma once

#include "motionbridge/core/types.hpp"

#include <optional>

namespace motionbridge {

// A future ros2_control SystemInterface owns an implementation of this boundary.
class IRosAdapter {
public:
    virtual ~IRosAdapter() = default;
    [[nodiscard]] virtual std::optional<MotionCommand> poll_command() = 0;
    virtual void publish_status(const ControllerStatus& status) = 0;
};

} // namespace motionbridge

