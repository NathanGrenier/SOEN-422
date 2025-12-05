# Embedded Firmware: Smart Bin Module
This directory contains the firmware source code for the IoT Smart Bin, built using PlatformIO and the Arduino Framework for the ESP32 (TTGO LoRa32).

The firmware is responsible for sensor data acquisition, power management, local actuation (Servo/LED), and communicating with the central server via MQTT.

# Hardware Components
The system is built on the TTGO LoRa32 v1 development board.

| Component           | Model / Spec               | Description                                            |
| ------------------- | -------------------------- | ------------------------------------------------------ |
| **MCU**             | TTGO LoRa32 (ESP32)        | Main controller with built-in WiFi & Battery circuit.  |
| **Distance Sensor** | HC-SR04                    | Measures distance to trash pile to calculate fill %.   |
| **Actuator**        | Hitec HS-422 Servo         | Opens/Closes the bin lid based on fill threshold.      |
| **Tilt Sensor**     | RBS 040100                 | Detects if bin is tipped over (e.g., during emptying). |
| **Battery**         | 18650 Li-Ion (Samsung 25R) | Power source.                                          |
| **Indicator**       | Red LED                    | Visual feedback for "Bin Full" state.                  |

## Wiring Diagram
[Fritzing Wiring Diagram File](<../static/Wiring Diagram.fzz>)

![alt text](<../static/Wiring Diagram.png>)

## Pin Configuration

Defined in `main.cpp`:
| Pin (GPIO) | Function                | Wire Color       |
| ---------- | ----------------------- | ---------------- |
| **17**     | Trigger (Ultrasonic)    | Yellow           |
| **16**     | Echo (Ultrasonic)       | White            |
| **13**     | Tilt Sensor             | White            |
| **12**     | Servo Data              | Yellow           |
| **14**     | Red LED                 | Blue             |
| **35**     | Battery Voltage Divider | (Internal/Board) |

# Firmware Logic

## Sampling Algorithm
To ensure data accuracy inside a trash bin (which has irregular surfaces), the firmware does not rely on a single sensor ping.

1. **Burst Mode:** Wakes up and takes 11 samples with a 60ms gap.
2. **Filtration:** Sorts readings and trims the top/bottom 25% (outliers).
3. **Averaging:** Calculates the mean of the remaining inner 50%.

## Power Management

- **Deep Sleep:** The device spends most of its time in Deep Sleep to conserve the 18650 battery.
- **Wake Triggers:**
  1. **Timer:** Wakes up every `CYCLE_INTERVAL_MS` (default 10s in Dev) to sample data.
  2. **Tilt Interrupt:** Wakes immediately if the bin is tipped over.
- **Modem Sleep:** WiFi radio is put to sleep between DTIM intervals when connected.

## Battery Monitoring
Voltage is read via pin 35. A lookup table based on the [Samsung INR18650-25R discharge curve (1C) [Page 6]](https://www.powerstream.com/p/INR18650-25R-datasheet.pdf) is used to map voltage (4.2V - 3.1V) to a precise percentage (100% - 0%).

# MQTT API
The device communicates via JSON payloads using a topic structure based on the unique Device ID (e.g., BIN_CI_001).

## Published by Device (Device -> Server)
| Topic Type             | Payload Example          | Description                                                    |
| ---------------------- | ------------------------ | -------------------------------------------------------------- |
| `bins/{id}/data`       | **(See JSON Below)**     | Main telemetry: fill level, battery status, and sensor states. |
| `bins/{id}/status`     | `"online"` / `"offline"` | Connectivity status (uses MQTT Last Will & Testament).         |
| `bins/{id}/get-config` | `{}`                     | Sent on startup to request the current threshold settings.     |


**Telemetry Payload (`bins/{id}/data`):**
```json
{
  "deviceId": "BIN_CI_001",
  "fillLevel": 45,        // Percentage (0-100)
  "batteryPercentage": 82,
  "voltage": 3.92,
  "isTilted": false       // True if currently being emptied
}
```
## Subscribed by Device (Server -> Device)
| Topic Type         | Description                                                              | Expected Payload      |
| ------------------ | ------------------------------------------------------------------------ | --------------------- |
| `cmd/ping/{id}`    | Forces the device to wake up, sample immediately, and report data.       | `N/A` (Empty payload) |
| `bins/{id}/config` | Updates the "Full" threshold dynamically (e.g., change from 85% to 95%). | `{"threshold": 95}`   |

# 🛠️ Development Setup

## Prerequisites
1. Install VS Code.
2. Install the PlatformIO extension.
3. Clone the repository.

## Configuration
1. Navigate to embedded/include/
2. Copy `credentials.h.example` to `credentials.h`
3. Update your WiFi and MQTT Broker credentials:
```cpp
#define WIFI_SSID "Your_SSID"
#define WIFI_PASSWORD "Your_Password"
#define MQTT_BROKER_URL "192.168.x.x" // Local IP of your PC running Mosquitto
```

## Building & Uploading
Connect your TTGO ESP32 via USB.

### For Development (Unsecured MQTT, Verbose Logging) 
Run the following task in PlatformIO:
```sh
PIO: Upload (env:development)
```

### For Production (Secure WSS, Optimized)
Run the following task in PlatformIO:
```sh
PIO: Upload (env:production)
```
