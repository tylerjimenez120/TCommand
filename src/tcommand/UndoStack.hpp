// UndoStack.hpp
//
// PROPÓSITO: Stack LIFO acotado de comandos ya ejecutados.
// El Executor empuja aquí los comandos exitosos y undoables.
// Cuando alguien llama undoLast(), se saca el más reciente y se
// invoca su undo().
//
// Características:
//   - Bounded: cuando se llena, descarta el más viejo (no crece infinito)
//   - Thread-safe (los productores son el Executor; el consumidor
//     puede ser un botón o un watchdog en otro thread)
//
// [TODO: implementar en la Parte 6]
/**
 * @file UndoStack.hpp
 * @brief Bounded LIFO stack of executed commands for undo support.
 *
 * Stores commands that completed successfully and are marked as undoable.
 * When the stack reaches capacity, the oldest entry is dropped to keep
 * memory bounded — a deliberate trade-off favouring deterministic memory
 * usage over unlimited undo history.
 *
 * Thread-safe: the Executor pushes from its worker thread while
 * undoLast() may be called from a UI or watchdog thread.
 */
#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

#include "ICommand.hpp"

namespace tcommand {

class UndoStack {
 public:
    explicit UndoStack(std::size_t capacity)
        : capacity_{capacity} {}

    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;
    UndoStack(UndoStack&&) = delete;
    UndoStack& operator=(UndoStack&&) = delete;

    /**
     * @brief Push a successfully-executed undoable command onto the stack.
     *
     * If the stack is at capacity, the oldest entry is discarded
     * (drop-oldest policy) before pushing the new one.
     */
    void push(std::unique_ptr<ICommand> cmd) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stack_.size() >= capacity_) {
            stack_.pop_front();  // drop oldest
        }
        stack_.push_back(std::move(cmd));
    }

    /**
     * @brief Pop the most recently pushed command.
     * @return The command, or nullptr if the stack is empty.
     */
    std::unique_ptr<ICommand> pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stack_.empty()) return nullptr;
        auto cmd = std::move(stack_.back());
        stack_.pop_back();
        return cmd;
    }

    /// Number of commands currently in the stack.
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.size();
    }

    /// Maximum number of commands the stack can hold.
    std::size_t capacity() const noexcept { return capacity_; }

    /// Whether the stack has no commands.
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stack_.empty();
    }

 private:
    mutable std::mutex mutex_;
    std::deque<std::unique_ptr<ICommand>> stack_;
    std::size_t capacity_;
};

}  // namespace tcommand