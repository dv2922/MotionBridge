#include "motionbridge/runtime/watchdog.hpp"

#include <cmath>
#include <stdexcept>

namespace motionbridge {

Watchdog::Watchdog(double timeout_seconds)
    : timeout_seconds_(timeout_seconds)
{
    if (!(timeout_seconds_ > 0.0) || !std::isfinite(timeout_seconds_)) {
        throw std::invalid_argument("Watchdog timeout must be positive and finite");
    }
}

void Watchdog::kick() noexcept
{
    elapsed_seconds_ = 0.0;
    armed_ = true;
}

void Watchdog::advance(double dt_seconds) noexcept
{
    if (armed_ && dt_seconds > 0.0 && std::isfinite(dt_seconds)) {
        elapsed_seconds_ += dt_seconds;
    }
}

void Watchdog::reset() noexcept
{
    elapsed_seconds_ = 0.0;
    armed_ = false;
}

bool Watchdog::expired() const noexcept
{
    return armed_ && elapsed_seconds_ >= timeout_seconds_;
}

double Watchdog::elapsed_seconds() const noexcept
{
    return elapsed_seconds_;
}

} // namespace motionbridge
