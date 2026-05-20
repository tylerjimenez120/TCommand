# TCommand — Command Design Pattern Lab (Embedded Actuator Controller)

Implementation of the **Command Design Pattern** in C++17 for embedded
systems (ESP32 / STM32) and real-time applications. The lab simulates an
industrial actuator controller (stepper motor + relay) with deferred
execution, undo support, and retry-with-exponential-backoff.

## 🎯 What this lab shows

How to encapsulate hardware operations as first-class command objects,
decoupling **what** must be done from **when** and **who** does it.
The pattern enables:

- **Deferred execution** — commands queued and executed by a worker thread
- **Undo** — successful commands stack up; revert in LIFO order
- **Retry with exponential backoff** — transient hardware errors handled
  automatically
- **Observability** — atomic counters for executed / retried / failed / undone

## 📦 What's included

### Hardware abstraction (`src/hal/`)
- `IHardwareAbstraction` — 4-primitive contract (writeRegister, readRegister,
  delayMs, millis) for portability
- `MockHal` — Linux-host implementation with thread-safe register bank,
  failure injection for testing, and configurable time-scale

### Command pattern core (`src/tcommand/`)
- `ICommand` — abstract command interface with execute / undo / isUndoable /
  isRetryable / name
- `CommandStatus` — `Success | TransientFailure | PermanentFailure`
- `CommandQueue` — thread-safe bounded FIFO (blocking push/pop + ISR-safe tryPush)
- `UndoStack` — bounded LIFO with drop-oldest policy
- `Executor` — single-threaded worker, exponential backoff retry, RAII lifecycle

### Concrete commands (`src/commands/`)
- `StepperMoveCommand` — moves stepper to target position
- `RelaySetCommand` — turns relay on/off

Both follow the **snapshot-before-mutate** pattern: capture current state in
`execute()` so `undo()` can restore it.

### Demo (`src/main.cpp`)
Runnable demo that wires everything up, runs a sequence of commands,
and exercises the undo path.

## 🛠️ Build & test

### Prerequisites
- Docker
- Docker Compose v2

### Steps

```bash
# Start the dev container
docker compose up -d
docker exec -it tcommand bash

# Inside the container:
mkdir -p build && cd build
cmake ..
make

# Run the test suite (27 tests across 6 suites)
./test/test_suite

# Run the demo
./src/tcommand_demo
```

## 📁 Layout

```
TCommand/
├── src/
│   ├── hal/                 # Hardware abstraction layer
│   │   ├── IHardwareAbstraction.hpp
│   │   └── MockHal.hpp
│   ├── tcommand/            # Command pattern core (generic)
│   │   ├── ICommand.hpp
│   │   ├── CommandQueue.hpp
│   │   ├── UndoStack.hpp
│   │   └── Executor.hpp
│   ├── commands/            # Concrete domain commands
│   │   ├── StepperMoveCommand.hpp
│   │   └── RelaySetCommand.hpp
│   └── main.cpp             # Demo executable
├── test/                    # GoogleTest suites
└── CMakeLists.txt
```

## 🔌 Porting to a real target

The HAL exposes only **four primitives**. Porting requires implementing them
against the vendor SDK:

| Target | writeRegister | readRegister | delayMs | millis |
|--------|--------------|-------------|---------|--------|
| ESP32 (ESP-IDF) | `REG_WRITE` | `REG_READ` | `vTaskDelay` | `esp_timer_get_time` |
| STM32 (HAL) | memory-mapped I/O | memory-mapped I/O | `HAL_Delay` | `HAL_GetTick` |

The rest of the codebase is target-agnostic.

## ⚙️ Threading model

- **One worker thread per Executor** — consumes the queue sequentially
- **Main thread** — produces commands without blocking (responsive UI / sensors / comms)
- **In real embedded** — `std::mutex` + `std::condition_variable` would be
  replaced by FreeRTOS primitives (`xQueueSend`, `xQueueReceive`) which
  encapsulate the same producer-consumer pattern

## 💡 Design decisions

### Why `unique_ptr<ICommand>` (heap)?
The polymorphic queue uses `std::unique_ptr` for ownership clarity. While
this implies heap allocation, the allocation happens only at command
creation time — not in the hot path. For strict zero-heap embedded systems,
object pools or static memory allocation would replace `make_unique`.

### Why `snapshot-before-mutate`?
Commands read current state in `execute()` BEFORE writing the new value.
This guarantees `undo()` can restore the exact previous state regardless
of what happened between execution and undo.

### Why a separate worker thread?
A long-running command (motor moving for 2 seconds) must not block the
main thread. The Executor pattern keeps the UI / sensor loop responsive
while commands run in the background.

## 🗺️ Lab roadmap (all merged)

| PR  | Scope |
|-----|-------|
| #1  | Project skeleton + HAL with mock implementation |
| #2  | ICommand + CommandQueue + UndoStack |
| #3  | Executor with exponential backoff |
| #4  | Concrete commands (Stepper + Relay) |
| #5  | Demo (main.cpp) + final README |

## ✅ Tests: 27/27 passing

- 6 MockHal — register R/W, failure injection, time primitives
- 4 CommandQueue — FIFO order, full-rejection, shutdown, multi-producer stress
- 4 UndoStack — LIFO order, drop-oldest, empty pop, capacity
- 7 Executor — retry-until-success, give-up after max, permanent/non-retryable
  short-circuit, undoable filtering, undoLast
- 3 StepperMoveCommand — execute, undo, undo-before-execute fails
- 3 RelaySetCommand — turn on/off, undo restores previous state

---

**Developed by:** Jesus Jimenez (Tyler Jmz)
**Systems & Data Engineer | IoT Edge Engineer**
