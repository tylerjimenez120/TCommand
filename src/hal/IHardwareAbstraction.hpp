/**
 * @file IHardwareAbstraction.hpp
 * @brief Hardware Abstraction Layer (HAL) contract for the TCommand lab.
 *
 * This interface decouples command implementations from the concrete target.
 * The same command code compiles for:
 *   - Linux host (using MockHal) for unit testing.
 *   - ESP32 (ESP-IDF) by implementing this interface against REG_WRITE/REG_READ.
 *   - STM32 (HAL) by implementing this interface against memory-mapped IO.
 *
 * Keeping the surface small (4 methods) is intentional: every additional
 * primitive is a portability cost.
 */
#ifndef TCOMMAND_HAL_IHARDWAREABSTRACTION_HPP_
#define TCOMMAND_HAL_IHARDWAREABSTRACTION_HPP_

#include <cstdint>

namespace tcommand::hal {

/**
 * @brief Abstract hardware interface used by all commands.
 *
 * Implementations must be thread-safe if commands are executed from
 * multiple threads. The MockHal provided in this lab uses an internal
 * mutex; bare-metal implementations on a single executor thread can
 * skip locking.
 */
class IHardwareAbstraction {
 public:
    IHardwareAbstraction() = default;
    virtual ~IHardwareAbstraction() = default;

    // No copying or moving: HAL instances own a hardware resource (a set of
    // registers, a peripheral). Copying would create two objects claiming
    // ownership of the same hardware, which is meaningless.
    IHardwareAbstraction(const IHardwareAbstraction&) = delete;
    IHardwareAbstraction& operator=(const IHardwareAbstraction&) = delete;
    IHardwareAbstraction(IHardwareAbstraction&&) = delete;
    IHardwareAbstraction& operator=(IHardwareAbstraction&&) = delete;

    /**
     * @brief Write a 32-bit value to a memory-mapped register.
     * @param addr  Register address.
     * @param value Value to write.
     * @return true on success, false on hardware error (NACK, timeout, etc.).
     */
    virtual bool writeRegister(uint32_t addr, uint32_t value) = 0;

    /**
     * @brief Read a 32-bit value from a memory-mapped register.
     * @param addr Register address.
     * @return Value read. Errors are signalled by side-channel (e.g. flag).
     */
    virtual uint32_t readRegister(uint32_t addr) const = 0;

    /**
     * @brief Block the calling thread/task for @p ms milliseconds.
     */
    virtual void delayMs(uint32_t ms) = 0;

    /**
     * @brief Monotonic millisecond counter since boot.
     */
    virtual uint64_t millis() const = 0;
};

}  // namespace tcommand::hal

#endif  // TCOMMAND_HAL_IHARDWAREABSTRACTION_HPP_