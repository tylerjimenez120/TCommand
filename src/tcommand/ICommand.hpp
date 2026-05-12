/**
 * @file ICommand.hpp
 * @brief Polymorphic Command interface (GoF Command Pattern).
 *
 * A Command encapsulates a request as an object: the receiver, the method,
 * and the parameters are bundled together so the request can be queued,
 * logged, retried, or undone. See README.md "Decision Matrix" for the
 * rationale of choosing this variant vs std::variant-based commands.
 *
 * Concrete commands that subclass ICommand are stored on the heap via
 * std::unique_ptr<ICommand>. The same concrete classes are also valid
 * alternatives for VariantCommand<...> because they expose the duck-typed
 * surface (execute / undo / name / isUndoable / isRetryable).
 */
#pragma once

#include <cstdint>
#include <string_view>

namespace tcommand {

/**
 * @brief Result of a command execution.
 *
 * Distinct from a boolean so that the Executor can decide whether
 * to retry, undo, or drop the command.
 */
enum class CommandStatus : uint8_t {
    Success = 0,       ///< Completed successfully.
    TransientFailure,  ///< Retryable (e.g. I2C NACK, SPI timeout).
    PermanentFailure   ///< Do not retry (e.g. invalid argument).
};

/**
 * @brief Abstract command interface for the polymorphic pipeline.
 *
 * Concrete commands hold their own context (target device, parameters,
 * pre-snapshot for undo). The queue stores them via std::unique_ptr<ICommand>;
 * once queued, ownership is unique.
 *
 * Why copy/move is enabled (rather than deleted): derived concrete commands
 * also need to be valid alternatives inside std::variant (used by the
 * heap-free pipeline in VariantCommand.hpp). std::variant requires its
 * alternatives to be at least move-constructible. Disabling those operators
 * on the base would prevent the same concrete class from being used in both
 * pipelines.
 *
 * Slicing concern: derived classes must not be copied through an ICommand
 * value. They are always stored either as concrete value types (variant)
 * or as std::unique_ptr<ICommand> (polymorphic queue). Plain ICommand
 * value copies do not occur in this codebase.
 */
class ICommand {
 public:
    ICommand() = default;
    virtual ~ICommand() = default;

    ICommand(const ICommand&) = default;
    ICommand& operator=(const ICommand&) = default;
    ICommand(ICommand&&) = default;
    ICommand& operator=(ICommand&&) = default;

    /**
     * @brief Perform the action.
     *
     * Implementations MUST capture any state needed for undo() before
     * mutating the hardware. After a successful execute(), undo() must
     * be able to revert the change.
     *
     * @return Success, TransientFailure (retryable), or PermanentFailure.
     */
    virtual CommandStatus execute() = 0;

    /**
     * @brief Revert a previously-executed action.
     *
     * Calling undo() before a successful execute() is a contract violation
     * and SHOULD return PermanentFailure. Idempotency is implementation-
     * defined: most commands restore a snapshot taken in execute().
     */
    virtual CommandStatus undo() = 0;

    /**
     * @brief Whether this command supports undo.
     *
     * Some commands (e.g. "send telemetry", "fire airbag") are inherently
     * irreversible. The Executor uses this flag to decide whether to push
     * the command onto the UndoStack after execution.
     *
     * Default: true. Override to return false for irreversible actions.
     */
    virtual bool isUndoable() const noexcept { return true; }

    /**
     * @brief Whether the Executor may retry on transient failure.
     *
     * Default: true. Override to return false for commands where retrying
     * is meaningless (e.g. an invalid-argument case that will always fail).
     */
    virtual bool isRetryable() const noexcept { return true; }

    /**
     * @brief Human-readable identifier for logging and debugging.
     *
     * Returned as std::string_view so derived classes can return a string
     * literal without allocating. Must remain valid for the lifetime of
     * the command.
     */
    virtual std::string_view name() const noexcept = 0;
};

}  // namespace tcommand