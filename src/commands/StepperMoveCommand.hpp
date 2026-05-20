/**
 * @file StepperMoveCommand.hpp
 * @brief Concrete ICommand: move a stepper motor to a target position.
 *
 * Demonstrates the canonical "snapshot before mutate" pattern that makes
 * undo() correct: the current position is read and stored before the new
 * setpoint is written, so undo() can restore it exactly.
 *
 * Simulated register map (lab-specific addresses):
 *   0x40000010 - target position  (write)
 *   0x40000014 - current position (read)
 *   0x40000018 - control register (write, bit0 = enable)
 */
#pragma once

#include <cstdint>
#include <string_view>

#include "../hal/IHardwareAbstraction.hpp" //es la lógica que nos bindea con el hardware
#include "../tcommand/ICommand.hpp" //es el contrato que define qué ES un comando

/*
Usamos IHardwareAbstraction porque el comando usa la HAL para llegar al hardware.
Usamos ICommand porque el comando es un ICommand — cumple el contrato para que la Queue y el Executor lo manejen

ICommand          → "tú eres un comando"        (identidad)
IHardwareAbstraction → "tú puedes tocar hardware"  (acceso/capacidad)
*/

namespace tcommand::commands {

/*
Son las direcciones de los registros del dispositivo: las "puertas" por donde se escribe (enviar órdenes) y se lee (consultar estado). 
El fabricante define cada una en su datasheet.
*/

// direcciones de los registros del motor (y un bit de control).
inline constexpr uint32_t kStepperTargetReg = 0x40000010;  //dónde escribo la posición objetivo
inline constexpr uint32_t kStepperCurrentReg = 0x40000014; // dónde leo la posición actual
inline constexpr uint32_t kStepperControlReg = 0x40000018;  // dónde escribo el control (encender/apagar)
inline constexpr uint32_t kStepperEnableBit = 0x1; // qué bit activa el motor


/*
Un comando que mueve un motor. En execute() guarda la posición actual, escribe la nueva, activa el motor. 
En undo() restaura la posición guardada. Usa int32_t para posiciones (pueden ser negativas), cast a uint32_t para la HAL.
*/
class StepperMoveCommand final : public ICommand {
 public:
    StepperMoveCommand(hal::IHardwareAbstraction& hal, int32_t target_steps)
        : hal_{hal}, target_{target_steps} {} //recibe lo que necesita para ejecutar (HAL + objetivo).

    CommandStatus execute() override { //: guarda el estado actual para poder revertir después.
        /*
        Lee la posición actual, la guarda, escribe la nueva posición, activa el motor. 
        Si algo falla en hardware → TransientFailure. Si todo va bien → Success.
        */
        // Snapshot the current position BEFORE mutating anything.
        previous_ =
            static_cast<int32_t>(hal_.readRegister(kStepperCurrentReg));
        snapshot_taken_ = true;

        //Escribir la nueva posiciónes
        if (!hal_.writeRegister(kStepperTargetReg,
                                static_cast<uint32_t>(target_))) {
            return CommandStatus::TransientFailure;
        }//Si falla (devuelve false) → retorna TransientFailure (el Executor reintentará)
        
        // Encender el motor
        if (!hal_.writeRegister(kStepperControlReg, kStepperEnableBit)) {
            return CommandStatus::TransientFailure;
        }

        //Si llegamos aquí, ambas escrituras funcionaron → éxito.
        return CommandStatus::Success;
    }

    /*
    verifica que haya snapshot, restaura la posición vieja escribiéndola al registro. 
    Si no hay snapshot → PermanentFailure. Si falla escribir → TransientFailure.
    */
    CommandStatus undo() override {
        if (!snapshot_taken_) {
            return CommandStatus::PermanentFailure;
        }
        if (!hal_.writeRegister(kStepperTargetReg,
                                static_cast<uint32_t>(previous_))) {
            return CommandStatus::TransientFailure;
        }
        return CommandStatus::Success;
    }

    std::string_view name() const noexcept override {
        return "StepperMove";
    }

    int32_t target() const noexcept { return target_; }

 private:
    hal::IHardwareAbstraction& hal_; //acceso al hardware
    int32_t target_; // a donde mover
    int32_t previous_{0}; // posicion previa - para poder hacer undo
    bool snapshot_taken_{false}; // ya se guardo el comando previo?
};

}  // namespace tcommand::commands


/*
Dirección: kStepperTargetReg = 0x40000010 (nunca negativa)
Valor que llega: -50 (int32_t)
Se guarda como: 4,294,967,246 (uint32_t) — mismo patrón de bits
Al leer con int32_t: recuperas -50
*/