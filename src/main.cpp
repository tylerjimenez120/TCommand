/**
 * @file main.cpp
 * @brief Demo of the TCommand pattern lab.
 *
 * Wires up the HAL, queue, undo stack, and executor; runs a small
 * sequence of stepper and relay commands; prints stats and exercises
 * the undo path. Intended as a runnable smoke test that mirrors what
 * a real firmware main() would look like.
 */
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "commands/RelaySetCommand.hpp"
#include "commands/StepperMoveCommand.hpp"
#include "hal/MockHal.hpp"
#include "tcommand/CommandQueue.hpp"
#include "tcommand/Executor.hpp"
#include "tcommand/UndoStack.hpp"

using tcommand::CommandQueue;                       // FIFO thread-safe para encolar comandos
using tcommand::Executor;                            // Worker thread que consume la queue y ejecuta comandos
using tcommand::UndoStack;                           // LIFO bounded para guardar comandos exitosos (undo)
using tcommand::commands::kRelayStateReg;            // Dirección del registro de estado del relé
using tcommand::commands::kStepperTargetReg;        // Dirección del registro de posición objetivo del motor
using tcommand::commands::RelaySetCommand;           // Comando concreto: encender/apagar relé
using tcommand::commands::StepperMoveCommand;       // Comando concreto: mover motor a posición X

namespace {

/// Wait until the queue has drained and the worker finished the last command.
void waitForDrain(CommandQueue& q) {
    while (q.size() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void printStats(const Executor& exec, const char* label) {
    const auto& s = exec.stats();
    std::cout << "\n--- Stats after " << label << " ---\n";
    std::cout << "  executed: " << s.executed.load() << "\n";
    std::cout << "  retried:  " << s.retried.load() << "\n";
    std::cout << "  failed:   " << s.failed.load() << "\n";
    std::cout << "  undone:   " << s.undone.load() << "\n";
}

}  // namespace

int main() {
    std::cout << "=== TCommand Pattern Lab — Demo ===\n\n";

    // 1. Setup
    tcommand::hal::MockHal hal{0.0};   // time_scale=0 → no real delays
    CommandQueue queue{16};
    UndoStack undo{16};
    Executor exec{queue, undo, hal};

    exec.start();
    std::cout << "Executor started.\n";

    // 2. Push a sequence of commands
    std::cout << "\nPushing commands...\n";
    queue.push(std::make_unique<StepperMoveCommand>(hal, 100));
    std::cout << "  -> StepperMove to 100\n";

    queue.push(std::make_unique<RelaySetCommand>(hal, true));
    std::cout << "  -> RelaySet ON\n";

    queue.push(std::make_unique<StepperMoveCommand>(hal, 250));
    std::cout << "  -> StepperMove to 250\n";

    queue.push(std::make_unique<RelaySetCommand>(hal, false));
    std::cout << "  -> RelaySet OFF\n";

    waitForDrain(queue);

    // 3. Inspect hardware state
    std::cout << "\nHardware state after executing all commands:\n";
    std::cout << "  Stepper target: " << hal.readRegister(kStepperTargetReg)
              << "\n";
    std::cout << "  Relay state:    " << hal.readRegister(kRelayStateReg)
              << "\n";

    printStats(exec, "first batch");

    // 4. Undo last two commands
    std::cout << "\nUndoing last two commands...\n";
    exec.undoLast();   // undo RelaySet OFF → relay back to ON
    exec.undoLast();   // undo StepperMove 250 → target back to 100

    std::cout << "Hardware state after two undos:\n";
    std::cout << "  Stepper target: " << hal.readRegister(kStepperTargetReg)
              << "\n";
    std::cout << "  Relay state:    " << hal.readRegister(kRelayStateReg)
              << "\n";

    printStats(exec, "two undos");

    // 5. Clean shutdown
    exec.stop();
    std::cout << "\nExecutor stopped cleanly.\n";

    return 0;
}