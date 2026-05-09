/**
 * @file MockHal.hpp
 * @brief Linux-host implementation of IHardwareAbstraction for unit testing.
 *
 * Simulates a memory-mapped register bank using std::unordered_map and a
 * configurable failure injection mechanism so tests can exercise retry paths
 * deterministically.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "IHardwareAbstraction.hpp"

namespace tcommand::hal {

/**
 * @brief In-memory HAL for tests. Thread-safe.
 *
 * Features:
 *   - Backing store for register reads/writes.
 *   - Programmable failure on next N writes (for retry tests).
 *   - Real wall-clock millis() (uses std::chrono::steady_clock).
 *   - delayMs() uses std::this_thread::sleep_for; tests can shrink delays
 *     by passing a time-scaling factor at construction.
 */
class MockHal final : public IHardwareAbstraction {
 public:
    /**
     * @param time_scale Multiplier applied to delayMs(). 1.0 = real time,
     *                   0.0 = no delay (fast tests), 0.1 = 10x faster.
     */
    explicit MockHal(double time_scale = 1.0)
        : time_scale_{time_scale},
          start_{std::chrono::steady_clock::now()} {}

    bool writeRegister(uint32_t addr, uint32_t value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++write_count_;
        if (writes_to_fail_ > 0) {
            --writes_to_fail_;
            ++failed_writes_;
            return false;
        }
        registers_[addr] = value;
        return true;
    }

    uint32_t readRegister(uint32_t addr) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registers_.find(addr);
        return (it == registers_.end()) ? 0u : it->second;
    }

    void delayMs(uint32_t ms) override {
        const auto scaled =
            static_cast<uint64_t>(static_cast<double>(ms) * time_scale_);
        std::this_thread::sleep_for(std::chrono::milliseconds(scaled));
    }

    uint64_t millis() const override {
        const auto now = std::chrono::steady_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start_)
                .count());
    }

    // ------ Test helpers (not part of the HAL contract) ------

    /// Force the next @p n writeRegister calls to fail.
    void injectWriteFailures(uint32_t n) {
        std::lock_guard<std::mutex> lock(mutex_);
        writes_to_fail_ = n;
    }

    /// Total number of writeRegister() calls (success + failed).
    uint32_t writeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return write_count_;
    }

    /// Number of writeRegister() calls that returned false.
    uint32_t failedWriteCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return failed_writes_;
    }

 private:
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, uint32_t> registers_;
    uint32_t writes_to_fail_{0};
    uint32_t write_count_{0};
    uint32_t failed_writes_{0};
    double time_scale_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace tcommand::hal






