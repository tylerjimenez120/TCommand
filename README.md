# Command Design Pattern Lab — Embedded Actuator Controller

[TODO: complete in Part 13]

This lab demonstrates the Command pattern applied to an embedded actuator
controller (stepper motor + relay) featuring:

  - Thread-safe FIFO queue
  - Retry with exponential backoff
  - Bounded undo stack
  - Heap-free variant using std::variant
  - Hardware Abstraction Layer portable to ESP32/STM32

---

**Developed by:** Jesus Jimenez (Tyler Jmz)
**Systems & Data Engineer | IoT Edge Engineer**