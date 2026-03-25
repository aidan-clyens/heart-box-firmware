# Heart Boxes Firmware Design

## Table of Contents

- [Heart Boxes Firmware Design](#heart-boxes-firmware-design)
  - [Table of Contents](#table-of-contents)
  - [1. Introduction](#1-introduction)
  - [2. System Overview](#2-system-overview)
    - [2.1 Requirements](#21-requirements)
    - [2.2 High-Level Architecture](#22-high-level-architecture)
  - [3. Hardware Platform](#3-hardware-platform)
    - [3.1 ESP32-WROOM Microcontroller](#31-esp32-wroom-microcontroller)
    - [3.2 GPIO Assignments](#32-gpio-assignments)
    - [3.3 Electronic Circuit (PCB)](#33-electronic-circuit-pcb)
  - [4. Software Architecture](#4-software-architecture)
    - [4.1 RTOS Task Framework](#41-rtos-task-framework)
    - [4.2 State Machine](#42-state-machine)
    - [4.3 Event Flow Example](#43-event-flow-example)
  - [5. Connectivity](#5-connectivity)
    - [5.1 MQTT Broker](#51-mqtt-broker)
    - [5.2 Security](#52-security)
  - [6. Power Considerations](#6-power-considerations)
  - [7. Development Environment](#7-development-environment)
    - [7.1 Prerequisites](#71-prerequisites)
      - [7.1.1 Windows Installation](#711-windows-installation)
      - [7.1.2 Linux Installation](#712-linux-installation)
    - [7.2 Build Commands](#72-build-commands)
    - [7.3 VS Code Tasks](#73-vs-code-tasks)
  - [8. Bill of Materials (BOM)](#8-bill-of-materials-bom)
  - [9. Conclusion](#9-conclusion)


## 1. Introduction  

The *Heart Boxes* project is a connected IoT system designed to let couples to send real-time messages to each other. You each have a wooden box that contains:  

- A heart-shaped LED array  
- A button at the top
- A WiFi connected microcontroller

When one person presses the button on the box, the other box lights up its LED heart. Communication is exchanged via MQTT messages through AWS IoT Core.  

This document summarizes the firmware design for the microcontroller, including system architecture, RTOS tasks, communication protocols, electronic circuit design, and integration.

## 2. System Overview  

### 2.1 Requirements
- Integrated WiFi connectivity
- Secure MQTT communication via AWS IoT Core
- Real-time bidirectional messaging between paired devices
- Low-power operation with battery support
- LED indicators for power, WiFi connectivity, and message reception

### 2.2 High-Level Architecture  

```
┌───────────────┐       ┌───────────────┐
│   Heart Box 1 │◄─────►│   Heart Box 2 │
└───────┬───────┘       └───────┬───────┘
        │                       │
        ▼                       ▼
   AWS IoT Core MQTT Broker (TLS-secured)
```

Each box acts as both publisher and subscriber to a shared MQTT topic. Button events are published; LED events are triggered upon message reception.  

## 3. Hardware Platform  

### 3.1 ESP32-WROOM Microcontroller
**ESP32-WROOM** is used for the microcontroller for its:  
- Integrated WiFi and Bluetooth  
- Dual-core LX6 processor (240 MHz)  
- Low power modes for battery operation  
- Mature ecosystem and MQTT support  

### 3.2 GPIO Assignments  

| Component         | Pin   | Direction | Notes |
|-------------------|-------|-----------|-------|
| Push Button       | GPIO26 | Input     | Debounced ISR |
| Heart LED Array   | GPIO12 | Output    | 20-LED matrix |
| Green Status LED  | GPIO14 | Output    | WiFi connected |
| Red Status LED    | GPIO27 | Output    | Error state |

### 3.3 Electronic Circuit (PCB)

## 4. Software Architecture  

### 4.1 RTOS Task Framework  
The firmware uses **FreeRTOS** with a message-driven architecture. Each subsystem runs as a task, communicating via queues.  

- **State Machine Task** – orchestrates system states (Idle, Provisioning, WiFi Connected, AWS IoT Connected)  
- **WiFi Task** – manages STA/AP modes, provisioning via HTTP server, connectivity monitoring  
- **AWS IoT Task** – handles secure MQTT connection, publish/subscribe logic  
- **GPIO Task** – abstracts button input and LED control  
- **File System Task** – manages credential persistence in NVS  

This design ensures **decoupling**, **maintainability**, and **clear event flow**.  

### 4.2 State Machine  

**States:**  
- `STATE_IDLE` – device boot, check for stored WiFi credentials  
- `STATE_PROVISIONING` – AP mode + HTTP server for credential entry  
- `STATE_WIFI_CONNECTED` – STA mode, verify connectivity  
- `STATE_AWS_IOT_CONNECTED` – operational, publish/subscribe active  

**Transitions:**  
- Idle → Provisioning (no credentials)  
- Idle → WiFi Connected (credentials found)  
- WiFi Connected → AWS IoT Connected (ping success)  
- AWS IoT Connected → Idle (disconnect events)  

**LED Indicators:**  
- Off = Idle  
- Blinking = Provisioning  
- Solid = Connected to AWS IoT  

---

### 4.3 Event Flow Example  

**Button Press → Remote LED On**  
1. GPIO ISR detects button press  
2. Event posted to State Machine  
3. AWS IoT Task publishes MQTT message (`heartbox/1`)  
4. Partner device receives message via subscription  
5. GPIO Task lights LED heart array  

## 5. Connectivity  

### 5.1 MQTT Broker  
- **AWS IoT Core** selected for reliability, built-in TLS security, and ease of configuration  
- Devices authenticate via X.509 certificates  
- Topic structure: `heartbox/{device_id}`  

### 5.2 Security  
- Mutual TLS authentication  
- Device certificates stored securely in NVS  
- Root CA validation  

## 6. Power Considerations  

- ESP32 active mode: 160–260 mA  
- LED array (20 LEDs @ 20 mA each): ~400 mA  
- Battery options:  
  - 9V battery (0.8–3.4 hours depending on mode)  
  - AA NiMH pack (up to 100 hours idle, ~3.6 hours active LED use)  
- USB-C input with regulated 5V supply  

## 7. Development Environment  

- **ESP-IDF v4.4** toolchain  
- Build commands: `idf.py build`, `idf.py flash`, `idf.py monitor`  
- VS Code integration with tasks for build/flash/monitor  
- Certificates stored per-device in `certs/{DEVICE_NAME}/`  

### 7.1 Prerequisites

#### 7.1.1 Windows Installation

1. Download and install [ESP-IDF Tools Installer](https://dl.espressif.com/dl/esp-idf/?idf=4.4) (ESP-IDF v4.4)
2. Configure ESP-IDF environment variables (automatically set by installer)

#### 7.1.2 Linux Installation

### 7.2 Build Commands

- **Build:** `idf.py build`
- **Flash:** `idf.py -p COM4 flash` (adjust COM port as needed)
- **Monitor:** `idf.py -p COM4 monitor`
- **Build + Flash + Monitor:** `idf.py build; idf.py -p COM4 flash monitor`
- **Configure:** `idf.py menuconfig`

### 7.3 VS Code Tasks

Available tasks in the workspace:

- ESP32 - Build App
- ESP32 - Build, Flash, and Monitor App
- ESP32 - Reconfigure App
- ESP32 - Build Test App
- ESP32 - Build, Flash, and Monitor Test App

## 8. Bill of Materials (BOM)  

| Item            | Qty | Cost (CAD) | Notes |
|-----------------|-----|------------|-------|
| ESP32-WROOM     | 2   | $23.95     | WiFi-enabled MCU |
| LEDs (5mm)      | 500 | $16.59     | Heart array |
| Push Buttons    | 20  | $12.99     | Momentary switches |
| PCB             | 2   | Custom     | KiCad design |
| Wooden Box      | 2   | Custom     | CNC/Fusion 360 design |


## 9. Conclusion  

The *Heart Boxes* project demonstrates a complete IoT system design:  
- Secure cloud connectivity via AWS IoT Core
- Event-driven firmware architecture with FreeRTOS
- Hardware abstraction and modular task design

This project highlights engineering skills in **embedded systems architecture, IoT integration, and maintainable firmware design**. It serves as both a personal creative build and a professional showcase of system-level engineering expertise.  
