# 🔥 Smart Fire Detection & Extinguishing System (IoT)

**An automated, low-cost IoT solution designed to bridge the "dangerous response gap" in fire safety.**

[![Project Status](https://img.shields.io/badge/Status-Prototype-orange.svg)](#)
[![Tech](https://img.shields.io/badge/Platform-ESP32-blue.svg)](#)
[![Framework](https://img.shields.io/badge/Framework-Arduino%20/%20C++-green.svg)](#)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](#)

Developed as a collaborative project under the **Department of Mechatronics Engineering**, this system addresses the critical delay between fire ignition and human response. While conventional alarms only notify, this prototype **detects, alerts, and suppresses** fire autonomously.

---

## 🚀 Key Features

* **⚡ Dual-Factor Verification:** Integrates an IR flame sensor and MQ-series smoke sensor to minimize false positives through concurrent detection logic.
* **💧 Autonomous Suppression:** Instantaneous activation of a 5V DC pump via a dedicated relay module upon hazard confirmation.
* **📱 Cloud Connectivity:** Real-time critical alerts delivered via **Telegram Bot API** with synchronized NTP timestamps.
* **🌐 Hybrid Control Interface:**
    * **Web Dashboard:** A local web server hosted on the ESP32 for live status monitoring.
    * **Remote Commands:** Full manual override (Pump/Buzzer) via Telegram commands or the Web UI.
* **🛡️ Fail-Safe Connectivity:** Intelligent **Fallback Access Point (AP)** mode ensures local control even if the primary Wi-Fi network is unavailable.
* **🎯 Precision Logic:** Software-based debouncing ($50ms$) and confirmation intervals ($500ms$) ensure high system reliability.

---

## 🛠️ Technical Architecture

### Hardware Stack
| Component | Function |
| :--- | :--- |
| **ESP32 Dev Board** | Central Processing & Wi-Fi Gateway |
| **Flame Sensor** | Infrared-based fire detection |
| **MQ Smoke Sensor** | Gas/Smoke concentration monitoring |
| **5V Relay Module** | Power switching for the actuator |
| **5V DC Pump** | Active fire suppression |
| **Buzzer & LEDs** | Audiovisual status indicators |

[Image of ESP32 fire detection system wiring diagram]

### Software & Libraries
The firmware is developed in **C++** using the **Arduino framework**. Key libraries include:
* `WiFi.h` & `WebServer.h`: Network management and local UI hosting.
* `UniversalTelegramBot.h` & `ArduinoJson.h`: Secure bot communication and data parsing.
* `NTPClient.h`: Time synchronization for alert logging.

---
