/**
 * @file RelaySetCommand.hpp
 * @brief Concrete ICommand: turn a relay on or off.
 *
 * Simpler than StepperMoveCommand: the relay has only two states (on/off),
 * so the snapshot and undo logic are straightforward.
 *
 * Simulated register map (lab-specific address):
 *   0x40000020 - relay state (write: 0 = off, 1 = on)
 * 
 * Comando concreto que enciende/apaga un relé. Recibe true (on) o false (off), guarda el estado previo en execute(), 
 * escribe 1 o 0 al registro del relé, y puede revertir al estado anterior en undo(). 
 * Mismo patrón que StepperMoveCommand pero más simple (solo 2 estados, un registro).
 * 
 */
#pragma once

#include <cstdint>
#include <string_view>

#include "../hal/IHardwareAbstraction.hpp"
#include "../tcommand/ICommand.hpp"

namespace tcommand::commands {

inline constexpr uint32_t kRelayStateReg = 0x40000020;

class RelaySetCommand final : public ICommand {
 public:
    RelaySetCommand(hal::IHardwareAbstraction& hal, bool turn_on)
        : hal_{hal}, desired_{turn_on} {}

    CommandStatus execute() override {
        // Snapshot current state before mutating.
        previous_ = hal_.readRegister(kRelayStateReg);
        snapshot_taken_ = true;

        const uint32_t new_state = desired_ ? 1u : 0u;
        if (!hal_.writeRegister(kRelayStateReg, new_state)) {
            return CommandStatus::TransientFailure;
        }
        return CommandStatus::Success;
    }

    CommandStatus undo() override {
        if (!snapshot_taken_) {
            return CommandStatus::PermanentFailure;
        }
        if (!hal_.writeRegister(kRelayStateReg, previous_)) {
            return CommandStatus::TransientFailure;
        }
        return CommandStatus::Success;
    }

    std::string_view name() const noexcept override {
        return "RelaySet";
    }

    bool desired() const noexcept { return desired_; }

 private:
    hal::IHardwareAbstraction& hal_;
    bool desired_;
    uint32_t previous_{0};
    bool snapshot_taken_{false};
};

}  // namespace tcommand::commands