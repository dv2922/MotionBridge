#pragma once

#include "motionbridge/interfaces/plc_interface.hpp"
#include "motionbridge/runtime/latest_value_mailbox.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>

namespace motionbridge {

class PlcCommunicationWorker {
public:
    struct Configuration {
        std::chrono::milliseconds poll_period{20};
        std::chrono::milliseconds reconnect_period{500};
    };

    struct Statistics {
        std::uint64_t successful_reads{0};
        std::uint64_t successful_writes{0};
        std::uint64_t communication_errors{0};
    };

    using CommandSnapshot = LatestValueMailbox<PlcCommandData>::Snapshot;

    PlcCommunicationWorker(Configuration configuration, IPlcInterface& plc);
    ~PlcCommunicationWorker();

    PlcCommunicationWorker(const PlcCommunicationWorker&) = delete;
    PlcCommunicationWorker& operator=(const PlcCommunicationWorker&) = delete;

    void start();
    void stop() noexcept;

    [[nodiscard]] std::optional<CommandSnapshot> try_read_command() const noexcept;
    bool try_publish_status(const PlcStatusData& status) noexcept;

    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] Statistics statistics() const noexcept;

private:
    void run(std::stop_token stop_token) noexcept;

    Configuration configuration_;
    IPlcInterface& plc_;
    mutable LatestValueMailbox<PlcCommandData> command_mailbox_;
    LatestValueMailbox<PlcStatusData> status_mailbox_;
    std::jthread thread_;
    std::atomic_bool connected_{false};
    std::atomic_uint64_t successful_reads_{0};
    std::atomic_uint64_t successful_writes_{0};
    std::atomic_uint64_t communication_errors_{0};
};

} // namespace motionbridge
