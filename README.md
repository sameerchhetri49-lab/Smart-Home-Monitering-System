# Smart Home Monitoring System
## An IoT and Edge AI–Enabled Home Safety and Automation System

The Smart Home Monitoring System (SHM) is an IoT-based home safety and automation project developed for the **COM6017M** module.  
It combines real-time sensing, edge intelligence using a Raspberry Pi, cloud analytics via ThingSpeak and MATLAB, and Telegram-based notifications to enhance household safety and automate responses to hazardous conditions.

---

## System Architecture

Sensors → Arduino UNO → BLE →  
Raspberry Pi (Edge Intelligence) →  
ThingSpeak Cloud (MATLAB Analytics) →  
Telegram Bot → User

---

## Key Features

- Real-time monitoring of environmental and safety conditions  
- Edge intelligence processing on Raspberry Pi  
- Automated control of door and clothes protection system  
- Local alerts using buzzer and LEDs  
- Cloud analytics using ThingSpeak and MATLAB  
- Telegram notifications with anti-spam logic  
- Operates reliably during intermittent internet connectivity  

---

## Hardware Used

- Arduino UNO  
- Raspberry Pi 4  
- MQ-2 Gas Sensor  
- Flame Sensor  
- DHT11 temperature & humidity sensor  
- Rain Sensor  
- Ultrasonic Sensor  
- Servo Motors (x2)  
- LEDs and Buzzer  
- Regulated USB Power Supply  

---

## Sensors & Actuators

- **MQ-2 Gas Sensor** – Detects gas leakage  
- **Flame Sensor** – Detects fire  
- **DHT11** – Temperature and humidity monitoring  
- **Rain Sensor + Servo Motor** – Protects clothes from rain  
- **Ultrasonic Sensor + Servo Motor** – Automated door control  
- **LEDs & Buzzer** – Local visual and audible alerts  

---

## Software & Technologies

- Arduino IDE  
- Python 3.11  
- TensorFlow Lite (lightweight / future-ready)  
- ThingSpeak (Cloud storage and visualization)  
- MATLAB Analytics  
- Telegram Bot API  
- Bluetooth Low Energy (BLE)  

---

## Repository Structure

```
SHM-IoT-Edge-AI/
├── Arduino/
│   └── smarthome.ino
├── RaspberryPi/
│   └── edgeai_auto.py
├── MATLAB/
│   └── matlab.m
├── images/
├── requirements.txt
└── README.md
```

---

## Edge Intelligence Description

The Edge Intelligence module runs locally on the Raspberry Pi to:
- Interpret sensor data received via BLE  
- Detect hazardous conditions locally  
- Trigger immediate alerts and automation  
- Reduce latency and cloud dependency  

This enables low-latency decision making without relying fully on the cloud.  
The system is designed to support future expansion to advanced ML models.

---

## Python Virtual Environment Setup (Raspberry Pi)

Create and activate a virtual environment:

```bash
python3 -m venv ai_env
source ai_env/bin/activate
```

Install dependencies:

```bash
pip install -r requirements.txt
```

---

## Start Smart Home System (Raspberry Pi)

Run the following commands on the Raspberry Pi terminal:

```bash
cd ~/SHM-IoT-Edge-AI
source ai_env/bin/activate
pip install -r requirements.txt
python RaspberryPi/edgeai_auto.py
```

---

## Once running, the system will:

- Receive sensor data via BLE  
- Process data locally on Raspberry Pi  
- Upload data to ThingSpeak  
- Trigger MATLAB analytics  
- Send Telegram alerts when events occur  
- Control actuators (servo motors, buzzer, LEDs)  

---

## Telegram Notifications

Telegram is used as the user notification layer.  
Alerts are sent only when the system state changes to prevent notification spam.

---

## HOW React, ThingHTTP & Telegram CONNECT (MENTAL MODEL)

```
MATLAB Analytics
        ↓
   Alert Detection
        ↓
ThingSpeak React
        ↓
   ThingHTTP Trigger
        ↓
Telegram Bot API
        ↓
      User
```

React = decision maker  
ThingHTTP = messenger  
Telegram = delivery platform  

---

## Project Screenshots

Screenshots of the ThingSpeak dashboard, Telegram notifications, and the physical system setup are available in the `images/` directory.

---

## Security Note

API keys, Telegram bot tokens, and cloud credentials are not included in this repository.  
Users must insert their own credentials using placeholder values before deployment.

---

## Author

**Sameer Chhetri**  
BSc Computer Science  
York St John University

Module: COM6017M – Internet of Things & Edge Intelligence
