# AEGIS Firmware

This folder contains the ESP32-S3 FreeRTOS firmware and Wokwi hardware configuration for the AEGIS Avionics Flight Control System.

## Files

- `main.cpp` — main FreeRTOS application
- `diagram.json` — Wokwi ESP32-S3 circuit configuration
- `libraries.txt` — Wokwi library list, if required

## Platform

- ESP32-S3
- ESP-IDF
- FreeRTOS
- Wokwi simulator

## Running the Project

1. Open the AEGIS Wokwi project.
2. Confirm the project is named:

   `McAgy-FINAL-RTS26Summer`

3. Start the simulation.
4. Observe the startup and health-monitor logs.
5. Press the GPIO 18 button to trigger the interrupt-response paths.

## Expected Output

The application reports:

- Task heartbeat counts
- Worst-case execution times
- Semaphore interrupt latency
- Direct-notification interrupt latency
- Cockpit command events
- Overall system status
