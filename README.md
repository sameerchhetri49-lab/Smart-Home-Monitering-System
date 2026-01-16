**Smart Home Monitoring System
An IoT and Edge AI–Enabled Home Safety and Automation System**
O**verview** 

The Smart Home Monitoring System (SHM) is an IoT-based home safety and automation project developed for the COM6017M – Internet of Things & Edge Intelligence module.
The system combines real-time sensing, edge-level intelligence using a Raspberry Pi, cloud analytics via ThingSpeak and MATLAB, and Telegram-based user notifications to enhance household safety and automate responses to hazardous conditions.

**System Architecture**

Sensors → Arduino UNO → BLE → Raspberry Pi (Edge Intelligence) → ThingSpeak Cloud (MATLAB Analytics) → Telegram Bot → User

The architecture ensures low-latency local decisions while still enabling cloud-based monitoring and analytics.

**Sensors & Actuators**

MQ-2 Gas Sensor – Detects gas leakage
Flame Sensor – Detects fire
DHT11 – Temperature and humidity monitoring
Rain Sensor + Servo Motor – Protects clothes from rain
Ultrasonic Sensor + Servo Motor – Automated door control
LEDs & Buzzer – Local visual and audible alerts

**Key Features**

Real-time monitoring of environmental and safety conditions
Edge intelligence processing on Raspberry Pi
Automated control of door and clothes protection system
Local alerts using buzzer and LEDs
Cloud analytics using ThingSpeak and MATLAB
Telegram notifications with anti-spam logic
Operates reliably during intermittent internet connectivity

**Hardware Used**

Arduino UNO
Raspberry Pi 4
MQ-2 Gas Sensor
Flame Sensor
DHT11 Sensor
Rain Sensor
Ultrasonic Sensor
Servo Motors
LEDs and Buzzer
Regulated USB Power Supply

**Software & Technologies**

Arduino IDE – Microcontroller programming (Arduino C)
Python 3.11 – Raspberry Pi processing
TensorFlow Lite (lightweight / future-ready) – Edge intelligence
ThingSpeak – Cloud storage and visualization
MATLAB Analytics – Data analysis and event logic
Telegram Bot API – User notifications
Bluetooth Low Energy (BLE) – Arduino to Raspberry Pi communication

**Repository Structure**

SHM-IoT-Edge-AI/
├── Arduino/
│   └── smarthome.ino
├── RaspberryPi/
│   └── edgeai_auto.py
├── MATLAB/
│   └── matlab.m
├── images/
│   └── system_setup.png
├── requirements.txt
└── README.md

**Edge Intelligence Description**

Edge intelligence is implemented on the Raspberry Pi to:
Interpret sensor data received via BLE
Detect hazardous conditions locally
Trigger immediate alerts and automation
Reduce latency and cloud dependency

The system is designed to support future expansion to advanced ML models while currently using efficient local decision logic suitable for edge devices.

Python Virtual Environment Setup (Raspberry Pi)
Create and activate a virtual environment:
python3 -m venv ai_env
source ai_env/bin/activate

Install dependencies:
pip install -r requirements.txt

Running the System (Raspberry Pi)
cd SHM-IoT-Edge-AI
source ai_env/bin/activate
python RaspberryPi/edgeai_auto.py

Once running, the system will:
Receive sensor data via BLE
Process data locally on Raspberry Pi
Upload data to ThingSpeak
Trigger MATLAB analytics
Send Telegram alerts when events occur
Control actuators (servo motors, buzzer, LEDs)

Telegram Notifications
Telegram is used as the user notification layer.
Alerts are sent only when system state changes

Prevents notification spam

Clear and concise hazard messages

Notification Flow:
Edge Detection → ThingSpeak → MATLAB → React → ThingHTTP → Telegram Bot → User

Project Media
ThingSpeak dashboard screenshots
Telegram alert screenshots
Physical system setup images
Available in the images/ directory.

Security Note
API keys, Telegram bot tokens, and cloud credentials are not included in this repository.
 Users must insert their own credentials using placeholder values before deployment.

Author
Sameer Chhetri
 BSc Computer Science
 York St John University
Module: COM6017M – Internet of Things & Edge Intelligence


