SparkDash: External Hardware Resource Monitor
Overview
A compact, plug-and-play external hardware monitor built specifically for the NVIDIA Spark workstation. It provides real-time tracking of critical system metrics — VRAM usage (Used/Total GiB), GPU temperature (°C), and system RAM utilization (%) — displayed on a crisp OLED screen.

The device connects via a single Type-C USB cable, which handles both power delivery and serial data transmission.

Key Features

Real-Time Telemetry: Seamlessly fetches accurate data directly from the NVIDIA GPU via nvidia-smi and system RAM via the psutil library.

Smart UI Layout: Implements dynamic font scaling. The C++ sketch automatically calculates string length and adjusts the layout to ensure that long VRAM values (e.g., "128/128 GiB") fit perfectly without breaking the 128x64 pixel grid.

Reliable Background Daemon: Runs as a lightweight, auto-recovering systemd service on the Linux host. It starts automatically with the OS and handles accidental USB cable disconnects gracefully.

Hardware Stack

Microcontroller: Arduino Nano V3 (Type-C, ATmega328P).

Display: 0.96" OLED Display (I2C, SSD1306 controller, 128x64, White pixels).

Enclosure: Custom modular 3D-printed case (black and white styling).

Assembly: Ultra-compact "sandwich" (back-to-back) design. Components are directly soldered using flexible wires without pin headers to minimize thickness, with double-sided foam tape serving as an insulator.

Software Stack

Host PC (Linux): Python 3 (pyserial, psutil, subprocess).

Microcontroller: C++ / Arduino IDE (Adafruit GFX & Adafruit SSD1306 libraries).
<img width="473" height="572" alt="IMG_20261013_203938_520" src="https://github.com/user-attachments/assets/c3f97da0-aa5f-48c5-a231-a7742b1e69e3" />
<img width="1664" height="1254" alt="20261013_192956" src="https://github.com/user-attachments/assets/57d75446-8ba5-4257-b8a0-aa69a4dde472" />
