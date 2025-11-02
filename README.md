# Smart-Fire-Detection-and-Extinguishing-System
This repository contains the code and documentation for a Smart Fire Detection and Extinguishing System.  The system is designed as a low-cost, automated Internet of Things (IoT) prototype to address the significant safety challenges posed by fire outbreaks. A team project designed under the department of Mechatronics Engineering
The system is designed as a low-cost, automated Internet of Things (IoT) prototype to address the significant safety challenges posed by fire outbreaks. It aims to close the "dangerous response gap" left by conventional manual extinguishers or basic alarms by providing a solution that both detects fire early and acts immediately to suppress it.





Key Features

Dual-Sensor Detection: Integrates both a flame sensor and an MQ-series smoke sensor to reliably monitor for fire conditions.





Automated Extinguishing: Automatically triggers a 5V DC pump via a relay module to begin extinguishing the fire immediately upon confirmed detection.





IoT Connectivity & Alerts: Uses the ESP32's built-in Wi-Fi to send real-time critical alert messages to a predefined chat ID via a Telegram Bot.




Remote & Local Control: Features a web interface hosted directly on the ESP32, allowing for live system status monitoring and remote manual control.




Manual Override: Allows users to manually activate or deactivate the pump and buzzer via the web interface or through Telegram commands.


Robust Connectivity: Includes a fallback Access Point (AP) mode that activates if a connection to the configured Wi-Fi network fails, ensuring local control is always available.



False Alarm Reduction: Implements software-based sensor debouncing (50ms) and a confirmation interval (500ms) that requires both sensors to detect a hazard, reducing false alarms.

Core Technology
The prototype is built on an ESP32 microcontroller and integrates sensing, processing, and actuation.


Hardware Components:


Controller: ESP32 Development Board 


Sensors: Flame Sensor , MQ-series Smoke Sensor 




Actuators: 5V Relay Module , 5V DC Pump , Buzzer 




Indicators: Red and Yellow LEDs for visual status 


Software & Logic:


Platform: Developed in C++ using the Arduino framework.


Key Libraries:


WiFi.h & WebServer.h: For network connection and hosting the local web dashboard.



UniversalTelegramBot.h & ArduinoJson.h: For handling Telegram bot communication.



NTPClient.h: For time synchronization to timestamp alerts.


System Logic

Initialization: On startup, the ESP32 attempts to connect to a pre-configured Wi-Fi network.


Fallback Mode: If the Wi-Fi connection fails, the system automatically creates its own access point named "FlameGuard-AP" for local control.


Monitoring: The system continuously reads data from the flame and smoke sensors.


Confirmation: To prevent false positives, a fire event is only confirmed when both sensors consistently detect a hazard within the defined confirmation interval.

Action (Automatic Mode): When a fire is confirmed, the system immediately:

Activates the audible buzzer and visual LED alerts.

Triggers the 5V relay, which energizes the DC water pump to begin suppression.

Sends a critical alert message via the Telegram bot, including sensor status and a timestamp.

Updates the status on the web dashboard.
