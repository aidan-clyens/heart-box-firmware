# Heart Boxes Firmware Design

---

## 1. Introduction  

The *Heart Boxes* project is a connected IoT system designed to enable couples to send symbolic “heart” messages to each other in real time. Each box contains:  

- A heart-shaped LED array  
- A single pressable button  
- An ESP32 microcontroller with integrated WiFi  

When one partner presses the button, the other partner’s box lights up its LED heart. Communication is achieved via MQTT messages exchanged through AWS IoT Core.  

This document outlines the firmware design for the ESP32 devices, including system architecture, task decomposition, communication protocols, and hardware integration.  

---

## 2. System Overview  

### 2.1 Functional Requirements  
- Integrated WiFi connectivity without external UI  
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

---

## 3. Hardware Platform  

### 3.1 Microcontroller Selection  
- **ESP32-WROOM** chosen for:  
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

---

## 4. Software Architecture  

### 4.1 Task Framework  
The firmware leverages **FreeRTOS** with a message-driven architecture. Each subsystem runs as a task, communicating via queues.  

- **State Machine Task** – orchestrates system states (Idle, Provisioning, WiFi Connected, AWS IoT Connected)  
- **WiFi Task** – manages STA/AP modes, provisioning via HTTP server, connectivity monitoring  
- **AWS IoT Task** – handles secure MQTT connection, publish/subscribe logic  
- **GPIO Task** – abstracts button input and LED control  
- **File System Task** – manages credential persistence in NVS  

This design ensures **decoupling**, **maintainability**, and **clear event flow**.  

---

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

---

## 5. Connectivity  

### 5.1 MQTT Broker  
- **AWS IoT Core** selected for reliability, built-in TLS security, and ease of configuration  
- Devices authenticate via X.509 certificates  
- Topic structure: `heartbox/{device_id}`  

### 5.2 Security  
- Mutual TLS authentication  
- Device certificates stored securely in NVS  
- Root CA validation  

---

## 6. Power Considerations  

- ESP32 active mode: 160–260 mA  
- LED array (20 LEDs @ 20 mA each): ~400 mA  
- Battery options:  
  - 9V battery (0.8–3.4 hours depending on mode)  
  - AA NiMH pack (up to 100 hours idle, ~3.6 hours active LED use)  
- USB-C input with regulated 5V supply  

---

## 7. Development Environment  

- **ESP-IDF v4.4** toolchain  
- Build commands: `idf.py build`, `idf.py flash`, `idf.py monitor`  
- VS Code integration with tasks for build/flash/monitor  
- Certificates stored per-device in `certs/{DEVICE_NAME}/`  

---

## 8. Bill of Materials (BOM)  

| Item            | Qty | Cost (CAD) | Notes |
|-----------------|-----|------------|-------|
| ESP32-WROOM     | 2   | $23.95     | WiFi-enabled MCU |
| LEDs (5mm)      | 500 | $16.59     | Heart array |
| Push Buttons    | 20  | $12.99     | Momentary switches |
| Wooden Box      | 2   | Custom     | CNC/Fusion 360 design |

---

## 9. Conclusion  

The *Heart Boxes* project demonstrates a complete IoT system design:  
- Secure cloud connectivity via AWS IoT Core  
- Event-driven firmware architecture with FreeRTOS  
- Hardware abstraction and modular task design  
- Practical power management for battery operation  

This project highlights engineering skills in **embedded systems architecture, IoT integration, and maintainable firmware design**. It serves as both a personal creative build and a professional showcase of system-level engineering expertise.  
