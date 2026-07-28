#pragma once

namespace motionbridge {

class Watchdog {
public:
    explicit Watchdog(double timeout_seconds);

    void kick() noexcept;
    void advance(double dt_seconds) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool expired() const noexcept;
    [[nodiscard]] double elapsed_seconds() const noexcept;

private:
    double timeout_seconds_;
    double elapsed_seconds_{0.0};
    bool armed_{false};
};

} // namespace motionbridge
