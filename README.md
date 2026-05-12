# TCommand — Command Design Pattern Lab (Embedded Actuator Controller)

A professional-grade implementation of the **Command Design Pattern** in C++17,
architected for embedded systems (ESP32 / STM32) and real-time applications.

> 🚧 **Work in progress** — this lab is being shipped incrementally as a series
> of stacked pull requests. See the [Roadmap](#roadmap) below.

## 🎯 Objective

Demonstrate how to encapsulate hardware operations as first-class command
objects, decoupling **what** must be done from **when** and **who** does it.
The lab simulates an industrial actuator controller (stepper motor + relay)
and ships features such as deferred execution, undo support, and
retry-with-exponential-backoff.

## 📦 Current scope (PR #1)

This first PR delivers the **project foundation**:

- Project skeleton with CMake build system
- Dockerized development environment (Debian Bookworm + GCC + clang tooling)
- Hardware Abstraction Layer (`IHardwareAbstraction`) with a Linux-host mock
  implementation (`MockHal`) for unit testing
- Failure injection mechanism for deterministic retry tests
- Time-scale primitive to keep tests fast
- Strict compile flags (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`...)
- `.clang-format` and `.clang-tidy` policies
- GoogleTest fetched via CMake `FetchContent`

## 🛠️ Build & test

### Prerequisites

- Docker
- Docker Compose v2

### Steps

```bash
# Build and start the dev container
docker compose up -d
docker exec -it tcommand bash

# Inside the container:
mkdir -p build && cd build
cmake ..
make

# Run the test suite
./test/test_suite

# Or via CTest:
ctest --output-on-failure
```

## 📁 Repository layout


TCommand/
├── src/
│   └── hal/
│       ├── IHardwareAbstraction.hpp   # HAL contract
│       └── MockHal.hpp                # Linux-host mock
├── test/
│   └── test_mock_hal.cpp              # MockHal unit tests
├── CMakeLists.txt
├── src/CMakeLists.txt
├── test/CMakeLists.txt
├── Dockerfile
├── compose.yml
├── .clang-format
├── .clang-tidy
└── README.md


## 🔌 Portability

The HAL exposes only **four primitives** (`writeRegister`, `readRegister`,
`delayMs`, `millis`). Porting to a real target requires implementing those
four against the vendor SDK:

- **ESP32 (ESP-IDF):** `REG_WRITE`, `REG_READ`, `vTaskDelay`, `esp_timer_get_time`
- **STM32 (HAL):** memory-mapped I/O, `HAL_Delay`, `HAL_GetTick`

The rest of the codebase is target-agnostic.

## 🗺️ Roadmap

| PR  | Status | Scope |
|-----|--------|-------|
| #1  | 🚧     | Project skeleton + HAL                 |
| #2  | ⏳     | ICommand + CommandQueue + UndoStack    |
| #3  | ⏳     | Executor with exponential backoff      |
| #4  | ⏳     | Concrete commands (Stepper + Relay)    |
| #5  | ⏳     | Heap-free `std::variant` + demo + bench|

---

**Developed by:** Jesus Jimenez (Tyler Jmz)
**Systems & Data Engineer | IoT Edge Engineer**
