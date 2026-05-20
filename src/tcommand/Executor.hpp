/**
 * @file Executor.hpp
 * @brief Worker that consumes the CommandQueue and applies retry policy.
 *
 * Architectural notes:
 *
 *   - The executor runs on a single worker thread (1 producer side, 1
 *     consumer here). On a real MCU this would be a FreeRTOS task with
 *     an xQueueReceive-style blocking dequeue.
 *
 *   - Retry policy is exponential backoff with a hard cap: delay grows as
 *     base_ms * 2^attempt up to max_delay_ms. This is the same algorithm
 *     used by lwIP TCP retransmission and most BLE stacks for connection
 *     attempts.
 *
 *   - Successful undoable commands are pushed onto an UndoStack. Failed
 *     commands are dropped (not undone) to avoid cascading partial undos.
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "CommandQueue.hpp"
#include "ICommand.hpp"
#include "UndoStack.hpp"
#include "../hal/IHardwareAbstraction.hpp"

namespace tcommand {

/**
 * @brief Configurable retry policy.
 *
 * Defaults are tuned for typical I2C/SPI peripherals: 3 attempts, starting
 * at 5ms, capped at 200ms. Override per-deployment.
 */
struct RetryPolicy {
    uint8_t max_attempts{3};
    uint32_t base_delay_ms{5};
    uint32_t max_delay_ms{200};
};

/**
 * @brief Statistics emitted by the executor for observability.
 */
struct ExecutorStats {
    std::atomic<uint64_t> executed{0};
    std::atomic<uint64_t> retried{0};
    std::atomic<uint64_t> failed{0};
    std::atomic<uint64_t> undone{0};
};

class Executor {
 public:
    Executor(CommandQueue& queue,
             UndoStack& undo_stack,
             hal::IHardwareAbstraction& hal,
             RetryPolicy policy = {})
        : queue_{queue},
          undo_stack_{undo_stack},
          hal_{hal},
          policy_{policy} {}

    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;
    Executor(Executor&&) = delete;
    Executor& operator=(Executor&&) = delete;

    ~Executor() { stop(); }

    /// Spin up the worker thread.
    void start() {
        running_.store(true);
        worker_ = std::thread([this] { run(); });
    }

    /// Signal stop, drain the queue, and join the worker.
    void stop() {
        if (!running_.exchange(false)) return;
        queue_.shutdown();
        if (worker_.joinable()) worker_.join();
    }

    /**
     * @brief Pop and undo the most recent undoable command.
     * @return true if something was undone.
     */
    bool undoLast() {
        auto cmd = undo_stack_.pop();
        if (!cmd) return false;
        const auto status = cmd->undo();
        if (status == CommandStatus::Success) {
            stats_.undone.fetch_add(1);
            return true;
        }
        return false;
    }

    const ExecutorStats& stats() const noexcept { return stats_; }

 private:
    void run() {
        while (running_.load()) {
            auto cmd = queue_.pop();
            if (!cmd) break;  // queue shut down
            executeWithRetry(std::move(cmd));
        }
    }

    void executeWithRetry(std::unique_ptr<ICommand> cmd) {
        for (uint8_t attempt = 0; attempt < policy_.max_attempts; ++attempt) {
            const auto status = cmd->execute();

            if (status == CommandStatus::Success) {
                stats_.executed.fetch_add(1);
                if (cmd->isUndoable()) {
                    undo_stack_.push(std::move(cmd));
                }
                return;
            }

            const bool give_up =
                status == CommandStatus::PermanentFailure ||
                !cmd->isRetryable() ||
                attempt + 1u == policy_.max_attempts;

            if (give_up) {
                stats_.failed.fetch_add(1);
                return;
            }

            stats_.retried.fetch_add(1);
            hal_.delayMs(backoffMs(attempt));
        }
    }

    /// delay = base * 2^attempt, capped at max.
    uint32_t backoffMs(uint8_t attempt) const noexcept {
        const uint32_t shifted = policy_.base_delay_ms << attempt;
        return (shifted > policy_.max_delay_ms) ? policy_.max_delay_ms
                                                : shifted;
    }

    CommandQueue& queue_;
    UndoStack& undo_stack_;
    hal::IHardwareAbstraction& hal_;
    RetryPolicy policy_;
    ExecutorStats stats_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

}  // namespace tcommand



/*
Executor = thread worker que saca comandos de la cola, los ejecuta con retry exponencial, y guarda los exitosos en el undo stack. Se apaga limpio con RAII.
*/