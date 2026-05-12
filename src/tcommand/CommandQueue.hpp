/**
 * @file CommandQueue.hpp
 * @brief Thread-safe bounded FIFO of unique_ptr<ICommand>.
 *
 * Producer/consumer queue inspired by FreeRTOS xQueue but built on
 * std::mutex + std::condition_variable so it runs unchanged on Linux for
 * unit testing. The bounded capacity models back-pressure: when the queue
 * is full, producers either block (push) or drop (tryPush) depending on
 * the policy chosen.
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

#include "ICommand.hpp"

namespace tcommand {

class CommandQueue {
 public:
    explicit CommandQueue(std::size_t capacity)
        : capacity_{capacity} {}

    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;
    CommandQueue(CommandQueue&&) = delete;
    CommandQueue& operator=(CommandQueue&&) = delete;

    /**
     * @brief Blocking push. Waits until space is available or shutdown().
     * @return true on success, false if the queue was shut down.
     */
    bool push(std::unique_ptr<ICommand> cmd) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] {
            return queue_.size() < capacity_ || shutdown_.load();
        });
        if (shutdown_.load()) return false;
        queue_.push(std::move(cmd));
        not_empty_.notify_one();
        return true;
    }

    /**
     * @brief Non-blocking push. Returns false if the queue is full.
     */
    bool tryPush(std::unique_ptr<ICommand> cmd) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_ || shutdown_.load()) return false;
        queue_.push(std::move(cmd));
        not_empty_.notify_one();
        return true;
    }

    /**
     * @brief Blocking pop. Waits until an item is available or shutdown().
     * @return The next command, or nullptr if shut down with empty queue.
     */
    std::unique_ptr<ICommand> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock,
                        [this] { return !queue_.empty() || shutdown_.load(); });
        if (queue_.empty()) return nullptr;
        auto cmd = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return cmd;
    }

    /**
     * @brief Wake up all waiters and prevent further pushes.
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_.store(true);
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::size_t capacity() const noexcept { return capacity_; }

    bool isShutdown() const noexcept { return shutdown_.load(); }

 private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<std::unique_ptr<ICommand>> queue_;
    std::size_t capacity_;
    std::atomic<bool> shutdown_{false};
};

}  // namespace tcommand