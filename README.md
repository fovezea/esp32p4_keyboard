# LinuxCNC Pendant (ESP32-P4)

A professional-grade pendant controller for LinuxCNC, built on the high-performance ESP32-P4 platform. This project aims to provide a robust and user-friendly interface for CNC machine operation, combining a modern touchscreen UI with tactile industry-standard controls.

## Features

- **Gmoccapy-Style Interface**: Intuitive screen-side soft keys for seamless navigation and control, inspired by the popular Gmoccapy LinuxCNC interface.
- **Physical Controls**: Integration of industry-standard hardware inputs for critical functions:
  - **Buttons**: Cycle Start, Feed Hold, Emergency Stop (E-Stop), Axis Selection, and more.
  - **Potentiometers**: Analog overrides for Feed Rate and Spindle Speed.
- **USB Host Support**: (Inherited from base) Capability to interface with external USB HID devices (Keyboards, Mice, MPGs) to extend functionality.
- **High Performance**: Leveraging the ESP32-P4 capabilities (MIPI-DSI/CSI, powerful CPU) for a responsive HMI experience.

## Hardware Requirements

- **MCU**: ESP32-P4 Development Board (e.g., Espressif EV Board).
- **Display**: MIPI-DSI Touchscreen.
- **Input Peripherals**:
  - Buttons/Switch Matrix.
  - Potentiometers/Encoders.
  - (Optional) External USB HID devices.

## Build and Flash

The project is built using the Espressif IoT Development Framework (ESP-IDF).

1. **Set up environment**: Ensure you have ESP-IDF v5.x installed.
2. **Configuration**:

    ```bash
    idf.py menuconfig
    ```

3. **Build**:

    ```bash
    idf.py build
    ```

4. **Flash and Monitor**:

    ```bash
    idf.py -p PORT flash monitor
    ```

## Pin Assignments

*Specific pin mappings for buttons, potentiometers, and display will be documented here as the hardware design is finalized.*
