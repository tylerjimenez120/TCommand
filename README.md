# Command Design Pattern Lab — Embedded Actuator Controller

[TODO: implementar en la Parte 13]

Este lab demuestra el patrón Command aplicado a un controlador
embebido de actuadores (motor stepper + relé) con:

  - Cola FIFO thread-safe
  - Retry con backoff exponencial
  - Stack de undo acotado
  - Variante heap-free con std::variant
  - Hardware Abstraction Layer portable a ESP32/STM32

---

**Developed by:** Jesus Jimenez (Tyler Jmz)
**Systems & Data Engineer | IoT Edge Engineer**
