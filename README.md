# High-Speed 16-IR Line Follower 🏎️⚡

<img width="1401" height="1401" alt="16-ir robot" src="https://github.com/user-attachments/assets/7e61c484-8e21-4061-8970-1945c763fe25" />


## 🏆 Project Overview

This repository contains the hardware design, custom PCBs, and C++ firmware for a high-speed, autonomous line-following robot. The system is engineered around the **STM32F103C8T6 (Blue Pill)** microcontroller and utilizes a dense **16-channel IR sensor array** to achieve millimeter-level error detection and ultra-smooth trajectory correction at high velocities.

## ⚙️ System Architecture & Tech Stack

By moving away from basic 8-bit microcontrollers and off-the-shelf modules, this project leverages custom hardware and a 32-bit ARM Cortex-M3 processor to handle complex floating-point math and high-frequency sensor polling without bottlenecking the main control loop.

* **Microcontroller:** STM32F103C8T6 (Blue Pill)
* **Firmware Language:** C++ (STM32 HAL)
* **Control Algorithm:** Tuned **PID (Proportional-Integral-Derivative)** Controller
* **Hardware Design:** Custom-designed Main Control Board & 16-IR Sensor Array PCBs
* **User Interface:** I2C OLED Display for real-time parameter tuning

### 🖥️ On-the-Fly Tuning (OLED Integration)

To eliminate the tedious process of hardcoding variables and constantly re-flashing the microcontroller during track testing, this robot features a custom onboard UI utilizing an **OLED display**. This allows for immediate, on-the-go diagnostics and optimization:

* **Live PID Tuning:** Adjust proportional ($K_p$), integral ($K_i$), and derivative ($K_d$) values directly on the starting grid.
* **Speed Profiling:** Modify base speeds and maximum cornering velocities instantly.
* **Telemetry & Lap Times:** View previous lap times immediately after completing a run to track optimization progress and benchmark different PID configurations.

### 🔌 Custom PCB Design

To ensure signal integrity at high speeds and eliminate messy wiring, I designed custom PCBs for this project:

1. **Main Control Board:** Integrates the STM32 microcontroller, motor drivers, OLED display, and power distribution into a single, compact footprint to lower the robot's center of gravity.
2. **16-IR Sensor Array Board:** Standard 5-IR arrays limit a robot to jerky, bang-bang control. By designing a custom 16-IR array, the microcontroller receives a highly granular, continuous gradient of the line's position. This allows the derivative ($K_d$) and proportional ($K_p$) terms to calculate precise motor speed adjustments, preventing overshoot even at top speeds.

> *(Consider dragging and dropping a 3D render or top-down photo of your PCBs here!)*

## 📂 Repository Structure

```text
├── firmware/            # STM32 C++ source code, headers, and .ioc configuration
├── hardware/            # PCB design files (Schematics, Layout, Gerbers)
├── cad/                 # 3D models (.step) for the chassis and sensor brackets
├── docs/                # Media and additional documentation
└── README.md
