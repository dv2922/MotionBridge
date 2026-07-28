#include "motionbridge/communication/plc_communication_worker.hpp"

#include <stdexcept>

namespace motionbridge {

PlcCommunicationWorker::PlcCommunicationWorker(
    Configuration configuration,
    IPlcInterface& plc)
    : configuration_(configuration)
    , plc_(plc)
{
    if (configuration_.poll_period <= std::chrono::milliseconds::zero()
        || configuration_.reconnect_period <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("PLC worker periods must be positive");
    }
}

PlcCommunicationWorker::~PlcCommunicationWorker()
{
    stop();
}

void PlcCommunicationWorker::start()
{
    if (thread_.joinable()) {
        return;
    }
    thread_ = std::jthread{[this](std::stop_token token) {
        run(token);
    }};
}

void PlcCommunicationWorker::stop() noexcept
{
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_.join();
    }
}

std::optional<PlcCommunicationWorker::CommandSnapshot>
PlcCommunicationWorker::try_read_command() const noexcept
{
    return command_mailbox_.try_read();
}

bool PlcCommunicationWorker::try_publish_status(const PlcStatusData& status) noexcept
{
    return status_mailbox_.try_publish(status);
}

bool PlcCommunicationWorker::connected() const noexcept
{
    return connected_.load(std::memory_order_relaxed);
}

PlcCommunicationWorker::Statistics PlcCommunicationWorker::statistics() const noexcept
{
    return {
        .successful_reads = successful_reads_.load(std::memory_order_relaxed),
        .successful_writes = successful_writes_.load(std::memory_order_relaxed),
        .communication_errors = communication_errors_.load(std::memory_order_relaxed),
    };
}

void PlcCommunicationWorker::run(std::stop_token stop_token) noexcept
{
    auto next_cycle = std::chrono::steady_clock::now();
    std::uint64_t last_status_sequence = 0;

    while (!stop_token.stop_requested()) {
        if (!connected_.load(std::memory_order_relaxed)) {
            if (!plc_.connect()) {
                ++communication_errors_;
                std::this_thread::sleep_for(configuration_.reconnect_period);
                continue;
            }
            connected_.store(true, std::memory_order_relaxed);
            next_cycle = std::chrono::steady_clock::now();
        }

        const auto command = plc_.read_command();
        if (!command) {
            ++communication_errors_;
            plc_.disconnect();
            connected_.store(false, std::memory_order_relaxed);
            continue;
        }
        if (command_mailbox_.try_publish(*command)) {
            ++successful_reads_;
        }

        if (const auto status = status_mailbox_.try_read();
            status && status->sequence != last_status_sequence) {
            if (plc_.write_status(status->value)) {
                last_status_sequence = status->sequence;
                ++successful_writes_;
            } else {
                ++communication_errors_;
                plc_.disconnect();
                connected_.store(false, std::memory_order_relaxed);
                continue;
            }
        }

        next_cycle += configuration_.poll_period;
        std::this_thread::sleep_until(next_cycle);
    }

    plc_.disconnect();
    connected_.store(false, std::memory_order_relaxed);
}

} // namespace motionbridge
