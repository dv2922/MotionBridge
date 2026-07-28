#pragma once

#include <chrono>
#include <functional>
#include <string_view>

namespace motionbridge {

// Keeps open62541-specific handles and types outside the control kernel.
class IOpcUaTransport {
public:
    using DataChangeCallback = std::function<void(std::string_view node, double value)>;

    virtual ~IOpcUaTransport() = default;
    virtual bool connect(std::string_view endpoint) = 0;
    virtual void disconnect() noexcept = 0;
    virtual bool subscribe(
        std::string_view node,
        std::chrono::milliseconds publishing_interval,
        DataChangeCallback callback) = 0;
    virtual bool write(std::string_view node, double value) = 0;
};

} // namespace motionbridge

