/**
 * @file test_executor_retry.cpp
 * @brief Unit tests for tcommand::Executor focused on retry, backoff,
 *        and undo flow.
 */
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "hal/MockHal.hpp"
#include "tcommand/CommandQueue.hpp"
#include "tcommand/Executor.hpp"
#include "tcommand/ICommand.hpp"
#include "tcommand/UndoStack.hpp"

namespace {

/// Build a RetryPolicy without C++20 designated initializers.
tcommand::RetryPolicy makePolicy(uint8_t attempts,
                                 uint32_t base_ms,
                                 uint32_t max_ms) {
    tcommand::RetryPolicy p;
    p.max_attempts = attempts;
    p.base_delay_ms = base_ms;
    p.max_delay_ms = max_ms;
    return p;
}

/**
 * @brief A command driven by a pre-defined script of return values.
 *
 * The script lists what execute() returns on each successive call:
 * script[0] is returned on the first call, script[1] on the second, etc.
 * Once the script is exhausted, Success is returned.
 */
class ScriptedCommand final : public tcommand::ICommand {
 public:
    explicit ScriptedCommand(std::vector<tcommand::CommandStatus> script,
                             bool undoable = true,
                             bool retryable = true)
        : script_{std::move(script)},
          undoable_{undoable},
          retryable_{retryable} {}

    tcommand::CommandStatus execute() override {
        const auto idx = exec_calls_.fetch_add(1);
        return idx < script_.size() ? script_[idx]
                                    : tcommand::CommandStatus::Success;
    }

    tcommand::CommandStatus undo() override {
        undo_calls_.fetch_add(1);
        return tcommand::CommandStatus::Success;
    }

    bool isUndoable() const noexcept override { return undoable_; }
    bool isRetryable() const noexcept override { return retryable_; }
    std::string_view name() const noexcept override { return "Scripted"; }

    std::size_t executeCalls() const noexcept { return exec_calls_.load(); }
    std::size_t undoCalls() const noexcept { return undo_calls_.load(); }

 private:
    std::vector<tcommand::CommandStatus> script_;
    bool undoable_;
    bool retryable_;
    std::atomic<std::size_t> exec_calls_{0};
    std::atomic<std::size_t> undo_calls_{0};
};

/// Wait until the queue drains, then give the worker a moment to finish.
void waitForDrain(tcommand::CommandQueue& q) {
    while (q.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

}  // namespace

TEST(ExecutorTest, SuccessfulCommandIsPushedToUndoStack) {
    tcommand::hal::MockHal hal{0.0};
    tcommand::CommandQueue queue{4};
    tcommand::UndoStack undo{4};
    tcommand::Executor exec{queue, undo, hal};

    auto cmd = std::make_unique<ScriptedCommand>(
        std::vector{tcommand::CommandStatus::Success});
    auto* raw = cmd.get();

    exec.start();
    queue.push(std::move(cmd));
    waitForDrain(queue);
    exec.stop();

    EXPECT_EQ(raw->executeCalls(), 1u);
    EXPECT_EQ(undo.size(), 1u);
    EXPECT_EQ(exec.stats().executed.load(), 1u);
}

TEST(ExecutorTest, RetriesOnTransientFailureUntilSuccess) {
    tcommand::hal::MockHal hal{0.0};
    tcommand::CommandQueue queue{4};
    tcommand::UndoStack undo{4};
    tcommand::Executor exec{queue, undo, hal, makePolicy(3, 0, 0)};

    auto cmd = std::make_unique<ScriptedCommand>(std::vector{
        tcommand::CommandStatus::TransientFailure,
        tcommand::CommandStatus::TransientFailure,
        tcommand::CommandStatus::Success,
    });
    auto* raw = cmd.get();

    exec.start();
    queue.push(std::move(cmd));
    waitForDrain(queue);
    exec.stop();

    EXPECT_EQ(raw->executeCalls(), 3u);
    EXPECT_EQ(exec.stats().executed.load(), 1u);
    EXPECT_EQ(exec.stats().retried.load(), 2u);
    EXPECT_EQ(exec.stats().failed.load(), 0u);
}

TEST(ExecutorTest, GivesUpAfterMaxAttempts) {
    tcommand::hal::MockHal hal{0.0};
    tcommand::CommandQueue queue{4};
    tcommand::UndoStack undo{4};
    tcommand::Executor exec{queue, undo, hal, makePolicy(2, 0, 0)};

    auto cmd = std::make_unique<ScriptedCommand>(std::vector{
        tcommand::CommandStatus::TransientFailure,
        tcommand::CommandStatus::TransientFailure,
        tcommand::CommandStatus::TransientFailure,
    });
    auto* raw = cmd.get();

    exec.start();
    queue.push(std::move(cmd));
    waitForDrain(queue);
    exec.stop();

    EXPECT_EQ(raw->executeCalls(), 2u);
    EXPECT_EQ(exec.stats().failed.load(), 1u);
    EXPECT_EQ(undo.size(), 0u);  // failed commands are not pushed
}

TEST(ExecutorTest, PermanentFailureDoesNotRetry) {
    tcommand::hal::MockHal hal{0.0};
    tcommand::CommandQueue queue{4};
    tcommand::UndoStack undo{4};
    tcommand::Executor exec{queue, undo, hal, makePolicy(5, 0, 0)};

    auto cmd = std::make_unique<ScriptedCommand>(
        std::vector{tcommand::CommandStatus::PermanentFailure});
    auto* raw = cmd.get();

    exec.start();
    queue.push(std::move(cmd));
    waitForDrain(queue);
    exec.stop();

    EXPECT_EQ(raw->executeCalls(), 1u);  // no retry despite max_attempts=5
    EXPECT_EQ(exec.stats().retried.load(), 0u);
    EXPECT_EQ(exec.stats().failed.load(), 1u);
}

TEST(ExecutorTest, NonUndoableCommandIsNotPushed) {
    tcommand::hal::MockHal hal{0.0};
    tcommand::CommandQueue queue{4};
    tcommand::UndoStack undo{4};
    tcommand::Executor exec{queue, undo, hal};

    auto cmd = std::make_unique<ScriptedCommand>(
        std::vector{tcommand::CommandStatus::Success},
        /*undoable=*/false);

    exec.start();
    queue.push(std::move(cmd));
    waitForDrain(queue);
    exec.stop();

    EXPECT_EQ(exec.stats().executed.load(), 1u);
    EXPECT_EQ(undo.size(), 0u);
}

TEST(ExecutorTest, NonRetryableCommandFailsImmediately) {
    tcommand::hal::MockHal hal{0.0};
    tcommand::CommandQueue queue{4};
    tcommand::UndoStack undo{4};
    tcommand::Executor exec{queue, undo, hal, makePolicy(5, 0, 0)};

    auto cmd = std::make_unique<ScriptedCommand>(
        std::vector{tcommand::CommandStatus::TransientFailure},
        /*undoable=*/true,
        /*retryable=*/false);
    auto* raw = cmd.get();

    exec.start();
    queue.push(std::move(cmd));
    waitForDrain(queue);
    exec.stop();

    EXPECT_EQ(raw->executeCalls(), 1u);  // not retried
    EXPECT_EQ(exec.stats().failed.load(), 1u);
}

TEST(ExecutorTest, UndoLastInvokesUndoOnMostRecent) {
    tcommand::hal::MockHal hal{0.0};
    tcommand::CommandQueue queue{4};
    tcommand::UndoStack undo{4};
    tcommand::Executor exec{queue, undo, hal};

    auto cmd = std::make_unique<ScriptedCommand>(
        std::vector{tcommand::CommandStatus::Success});
    auto* raw = cmd.get();

    exec.start();
    queue.push(std::move(cmd));
    waitForDrain(queue);

    EXPECT_TRUE(exec.undoLast());
    EXPECT_EQ(raw->undoCalls(), 1u);
    EXPECT_FALSE(exec.undoLast());  // stack now empty

    exec.stop();
}