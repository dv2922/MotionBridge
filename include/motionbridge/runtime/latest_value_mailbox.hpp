#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace motionbridge {

template <typename Value>
class LatestValueMailbox {
public:
    struct Snapshot {
        Value value;
        std::uint64_t sequence;
    };

    bool try_publish(const Value& value) noexcept
    {
        std::unique_lock lock{mutex_, std::try_to_lock};
        if (!lock.owns_lock()) {
            return false;
        }
        value_ = value;
        ++sequence_;
        return true;
    }

    bool try_publish(Value&& value) noexcept
    {
        std::unique_lock lock{mutex_, std::try_to_lock};
        if (!lock.owns_lock()) {
            return false;
        }
        value_ = std::move(value);
        ++sequence_;
        return true;
    }

    [[nodiscard]] std::optional<Snapshot> try_read() const noexcept
    {
        std::unique_lock lock{mutex_, std::try_to_lock};
        if (!lock.owns_lock() || !value_) {
            return std::nullopt;
        }
        return Snapshot{*value_, sequence_};
    }

private:
    mutable std::mutex mutex_;
    std::optional<Value> value_;
    std::uint64_t sequence_{0};
};

} // namespace motionbridge
