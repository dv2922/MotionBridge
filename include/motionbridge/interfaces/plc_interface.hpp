#pragma once

#include "motionbridge/core/types.hpp"

#include <optional>

namespace motionbridge {

// Future Siemens or TwinCAT adapters translate their native data model here.
class IPlcInterface {
public:
    virtual ~IPlcInterface() = default;
    virtual bool connect() = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual std::optional<MotionCommand> read_command() = 0;
    virtual bool write_status(const ControllerStatus& status) = 0;
};

} // namespace motionbridge

