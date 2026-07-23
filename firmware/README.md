# AEGIS Avionics Flight Control System

## Overview

AEGIS is a FreeRTOS-based avionics simulation developed for the EEL 4775 Real-Time Systems course. The project demonstrates periodic task scheduling, interrupt handling, inter-task communication, and real-time performance measurement on an ESP32-S3 using Wokwi. The system simulates a simplified flight control environment with sensor monitoring, flight control, telemetry, and health monitoring tasks.

---

## Features

- Four periodic real-time tasks with fixed priorities
- GPIO interrupt service routine (ISR)
- Binary semaphore for command handling
- Direct task notification for emergency response
- Heartbeat monitoring for all tasks
- Worst-Case Execution Time (WCET) measurement
- Real-time serial console output
- Executed on an ESP32-S3 using FreeRTOS

---

## System Architecture

The system consists of four periodic avionics tasks managed by the FreeRTOS scheduler. A GPIO interrupt simulates an external cockpit event. The ISR signals two tasks using different synchronization mechanisms:

- Binary semaphore → Command Handler
- Direct task notification → Emergency Response

A Health Monitor task periodically reports heartbeat counts and WCET values for each task.

---

## Task Scheduling

| Task | Period | Priority | Purpose |
|------|---------|----------|---------|
| Attitude Sensor | 10 ms | High | Simulates aircraft sensor sampling |
| Flight Controller | 20 ms | Medium-High | Processes sensor data and flight control logic |
| Telemetry | 50 ms | Medium | Simulates communication with ground systems |
| Health Monitor | 100 ms | Low | Reports heartbeat counts and WCET statistics |

The scheduler uses fixed-priority preemptive scheduling provided by FreeRTOS.

---

## Interrupt Handling

A push button connected to GPIO 18 generates an external interrupt. The interrupt service routine records the event and immediately wakes waiting tasks using two synchronization methods:

- Binary semaphore
- Direct task notification

This demonstrates two common FreeRTOS mechanisms for responding to asynchronous hardware events.

---

## WCET Results

Worst-Case Execution Time (WCET) was measured during execution and reported by the Health Monitor task.

Approximate observed values:

| Task | WCET |
|------|------|
| Attitude Sensor | 346 µs |
| Flight Controller | 1993 µs |
| Telemetry | 18919 µs |
| Health Monitor | 28892 µs |

These measurements verify that all tasks execute within their scheduled timing requirements.

---

## Hazard Analysis

Several potential hazards were considered during development:

- Delayed interrupt handling could increase response latency.
- Missed synchronization events could prevent command processing.
- Excessive execution time could reduce system responsiveness.

The use of FreeRTOS scheduling, interrupt-driven synchronization, and runtime monitoring helps reduce these risks.

---

## Build Instructions

Requirements:

- PlatformIO
- ESP32-S3
- FreeRTOS
- Wokwi Simulator

Steps:

1. Open the project in VS Code with PlatformIO.
2. Build the project.
3. Launch the Wokwi simulation.
4. Open the serial monitor.
5. Press the simulated button to trigger interrupt events.

---

## Demo Video

The demonstration video shows:

- Project overview
- System startup
- Periodic task execution
- GPIO interrupt operation
- Semaphore and notification signaling
- Health Monitor reporting WCET and heartbeat statistics

---

## Reflection

This project provided practical experience implementing a real-time embedded system using FreeRTOS. It reinforced concepts including task scheduling, interrupt handling, synchronization mechanisms, timing analysis, and runtime performance monitoring. Developing and testing the system in Wokwi also demonstrated the importance of verifying timing behavior and ensuring predictable task execution in embedded applications.

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
