/**
 * @file TestHelpers.hpp
 * @brief Common test utilities shared across test files.
 */
#pragma once

#include <atomic>
#include <memory>
#include <string_view>
#include <vector>

#include "tcommand/ICommand.hpp"

namespace tcommand::test {

/**
 * @brief Minimal fake command for testing queue/stack ordering.
 *
 * Tagged with an id so tests can verify FIFO/LIFO behavior.
 */
class FakeCommand final : public ICommand {
 public:
    explicit FakeCommand(int id) : id_{id} {}

    CommandStatus execute() override { return CommandStatus::Success; }
    CommandStatus undo() override { return CommandStatus::Success; }
    std::string_view name() const noexcept override { return "Fake"; }

    int id() const noexcept { return id_; }

 private:
    int id_;
};

/**
 * @brief Scripted command for testing Executor retry logic.
 *
 * Returns a pre-defined sequence of CommandStatus values on successive
 * execute() calls.
 */
class ScriptedCommand final : public ICommand {
 public:
    explicit ScriptedCommand(std::vector<CommandStatus> script,
                             bool undoable = true,
                             bool retryable = true)
        : script_{std::move(script)},
          undoable_{undoable},
          retryable_{retryable} {}

    CommandStatus execute() override {
        const auto idx = exec_calls_.fetch_add(1);
        return idx < script_.size() ? script_[idx] : CommandStatus::Success;
    }

    CommandStatus undo() override {
        undo_calls_.fetch_add(1);
        return CommandStatus::Success;
    }

    bool isUndoable() const noexcept override { return undoable_; }
    bool isRetryable() const noexcept override { return retryable_; }
    std::string_view name() const noexcept override { return "Scripted"; }

    std::size_t executeCalls() const noexcept { return exec_calls_.load(); }
    std::size_t undoCalls() const noexcept { return undo_calls_.load(); }

 private:
    std::vector<CommandStatus> script_;
    bool undoable_;
    bool retryable_;
    std::atomic<std::size_t> exec_calls_{0};
    std::atomic<std::size_t> undo_calls_{0};
};

}  // namespace tcommand::test