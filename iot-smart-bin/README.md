# ♻️ IoT Smart Bin: SOEN 422 Course Project

 | Web CI & Deployment Status                                                                                                                                                       | Embedded CI Status                                                                                                                                                                                 |
 | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
 | [![Web CI & Deploy](https://github.com/NathanGrenier/SOEN-422/actions/workflows/web_ci.yaml/badge.svg)](https://github.com/NathanGrenier/SOEN-422/actions/workflows/web_ci.yaml) | [![Embedded CI](https://github.com/NathanGrenier/SOEN-422/actions/workflows/embedded_ci.yaml/badge.svg?branch=main)](https://github.com/NathanGrenier/SOEN-422/actions/workflows/embedded_ci.yaml) |

**Quick Links:** 
- [Live Demo Video]()
- [Project Presentation Slides](https://liveconcordia-my.sharepoint.com/:p:/g/personal/na_greni_live_concordia_ca/IQCI7jE1QCS0Q5j33NtqWKxFASTevuJy1LxLtF4jdW9WwDI?e=jMpBzi)
    > [!NOTE] 
    > You need a `concordia` university email to access this link.

## 📖 Project Overview

The IoT Smart Bin is a full-stack waste management solution designed to optimize collection routes and monitor fill levels in real-time. This system addresses inefficiencies in traditional waste collection by providing data-driven insights to campus management.

A sensor-equipped microcontroller in each bin measures the fill level and communicates this data to a central server. The server processes and stores this data, presenting it on a web-based dashboard that displays all bins, their current status, and fill levels.

## 🏗️ System Architecture

![alt text](<static/Diagram (4-12-2025).png>)

## ✨ Key Features

- **Real-Time Fill Monitoring:** Utilizes an HC-SR04 Ultrasonic sensor to measure waste levels with outlier filtration and averaging algorithms.
- **Automated Lid Actuation:** A servo-controlled lid automatically opens when waste is below capacity and locks/closes when the bin exceeds the defined threshold.
- **Smart Tilt Detection:** Integrated tilt sensor pauses sampling when the bin is being emptied to prevent false readings.
- **Battery Management:** Monitors Li-Ion battery voltage (Samsung 25R curve) and enters Deep Sleep modes to extend battery life, waking only for periodic cycles or critical events.
- [**Remote Dashboard:**](https://iot-smart-bin.ngrenier.com/) Live visualization of fill levels, battery status, and device health via a web interface.
- **Bidirectional MQTT Communication:** Sends telemetry data to the server and accepts remote commands (e.g., changing fill thresholds or forcing a ping) in real-time.

## 🛠️ Technology Stack

### Embedded System

- **Microcontroller:** TTGO LoRa32 (ESP32)
- **Framework:** Arduino (via PlatformIO)
- **Sensors & Actuators:** HC-SR04, Tilt Sensor, Servo Motor
- **Protocols:** MQTT / MQTT over WSS (Secure WebSockets)

### Web & Infrastructure

- **Frontend/Backend:** Node.js (Tanstack Start/React)
- **Database:** PostgreSQL (managed via Docker)
- **MQTT Broker:** Eclipse Mosquitto (MQTT Broker)
- **Containerization:** Docker & Docker Compose
- **CI/CD:** GitHub Actions (Automated Builds for Web & Firmware)

## 🔌 Wiring Diagram
[Fritzing Wiring Diagram File](<static/Wiring Diagram.fzz>)

![alt text](<static/Wiring Diagram.png>)

## 📦 Enclosure CAD Model

[Onshape CAD Assembly](https://cad.onshape.com/documents/af4b9174dbfab3ca97324e50/w/f59ea6e609bcd326ed7891fa/e/541e6e6be9eae6838506ab1d?renderMode=0&uiState=69330d3c9476828f5b520ddd
)

![alt text](<static/CAD Enclosure.png>)

## 🚀 Getting Started

Follow these instructions to get a local copy of the project up and running for development and testing.

### Prerequisites

- PlatformIO IDE (for VS Code)
- Node.js v18.x or later
- Docker & Docker Compose

### Hardware & Firmware

Follow the [README guide](/iot-smart-bin/embedded/README.md) in the `embedded/` folder.

### Web Application

Follow the [README guide](/iot-smart-bin/web/README.md) in the `web/` folder.