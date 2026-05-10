# 🌉 IoT Bridge Structural Health Monitoring System

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![LoRa](https://img.shields.io/badge/LoRa-SX1278-green.svg)](https://www.semtech.com/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A structural health monitoring system for bridges using IMU sensors, transmitting data via LoRa with AES encryption and displaying on a real-time web dashboard.

---

## 📋 Table of Contents

- [Overview](#-overview)
- [System Architecture](#-system-architecture)
- [Hardware Requirements](#-hardware-requirements)
- [Software Requirements](#-software-requirements)
- [Project Structure](#-project-structure)
- [Installation Guide](#-installation-guide)
  - [Step 1: MPU_DashBoard](#step-1-install-mpu_dashboard-web-server)
  - [Step 2: ESP_Receiver](#step-2-install-esp_receiver-master-node)
  - [Step 3: ESP_Sender](#step-3-install-esp_sender-slave-node)
- [System Verification](#-system-verification)
- [Advanced Configuration](#-advanced-configuration)
- [Troubleshooting](#-troubleshooting)
- [API Reference](#-api-reference)
- [Contributing](#-contributing)
- [License](#-license)

---

## 🎯 Overview

The IoT Bridge Monitoring System is designed to:

- **Collect data** from accelerometer/gyroscope sensors (MPU6050)
- **Transmit wirelessly** via LoRa with TDMA (Time Division Multiple Access) mechanism
- **Secure data** using AES-128 CBC + CMAC encryption
- **Store** in MongoDB database
- **Display** on a real-time web dashboard

### ✨ Key Features

| Feature | Description |
|---------|-------------|
| 🔐 **AES-128 Encryption** | Secure data transmission over LoRa and MQTT |
| 📡 **LoRa SX1278** | Long range, low power consumption |
| ⏱️ **TDMA** | Multi-node synchronization to avoid collisions |
| 📊 **Real-time Dashboard** | Visual data display with Highcharts |
| 🗄️ **MongoDB** | Flexible data storage, easy querying |
| 🔄 **FreeRTOS** | Multi-tasking on ESP32 |

---

## 🏗 System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SYSTEM ARCHITECTURE                               │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────┐      LoRa 433MHz      ┌──────────────┐       MQTT        ┌─────────────┐
│ ESP_Sender  │ ───────────────────►  │ ESP_Receiver │ ───────────────►  │   Server    │
│   (Slave)   │   Encrypted Data      │   (Master)   │   JSON Payload    │  Dashboard  │
│             │                       │              │                   │             │
│ ┌─────────┐ │                       │ ┌──────────┐ │                   │ ┌─────────┐ │
│ │ MPU6050 │ │                       │ │   WiFi   │ │                   │ │ MongoDB │ │
│ └─────────┘ │                       │ └──────────┘ │                   │ └─────────┘ │
│ ┌─────────┐ │                       │ ┌──────────┐ │                   │ ┌─────────┐ │
│ │ SX1278  │ │                       │ │  SX1278  │ │                   │ │ Web UI  │ │
│ └─────────┘ │                       │ └──────────┘ │                   │ └─────────┘ │
└─────────────┘                       └──────────────┘                   └─────────────┘
      │                                      │                                  │
      │              ┌───────────────────────┼──────────────────────────────────┘
      │              │                       │
      ▼              ▼                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Sensor Task ──► Encrypt Task ──► LoRa TX ──► LoRa RX ──► MQTT ──► MongoDB  │
│     100Hz           AES-128         TDMA       Decrypt     JSON     Store   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 🔧 Hardware Requirements

### ESP_Sender (Slave Node)

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 DevKit | 1+ | One board per node |
| SX1278 LoRa Module | 1+ | 433MHz |
| MPU6050 | 1+ | 6-axis sensor |
| Jumper wires | - | Dupont wires |

### ESP_Receiver (Master Node)

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 DevKit | 1 | With WiFi |
| SX1278 LoRa Module | 1 | 433MHz |

### Wiring Diagram

**SX1278 LoRa ↔ ESP32:**

```
┌──────────────┬──────────────┐
│  SX1278 Pin  │  ESP32 Pin   │
├──────────────┼──────────────┤
│     VCC      │    3.3V      │
│     GND      │    GND       │
│     NSS      │    GPIO 5    │
│     DIO0     │    GPIO 2    │
│     RESET    │    GPIO 14   │
│     SCK      │    GPIO 18   │
│     MISO     │    GPIO 19   │
│     MOSI     │    GPIO 23   │
└──────────────┴──────────────┘
```

**MPU6050 ↔ ESP32 (Sender only):**

```
┌──────────────┬──────────────┐
│  MPU6050 Pin │  ESP32 Pin   │
├──────────────┼──────────────┤
│     VCC      │    3.3V      │
│     GND      │    GND       │
│     SDA      │    GPIO 21   │
│     SCL      │    GPIO 22   │
└──────────────┴──────────────┘
```

---

## 💻 Software Requirements

### Development Tools

- **PlatformIO** - IDE for ESP32 ([Installation](https://platformio.org/install))
- **Node.js** v16+ - Runtime for server ([Download](https://nodejs.org/))
- **MongoDB** v5+ - Database ([Download](https://www.mongodb.com/try/download/community))

### ESP32 Libraries (auto-installed via PlatformIO)

- RadioLib
- Adafruit MPU6050
- Adafruit Unified Sensor
- PubSubClient
- mbedTLS (included in ESP-IDF)

### Node.js Libraries (installed via npm)

- express
- mongoose
- mqtt
- cors

---

## 📁 Project Structure

```
design_system_iot/
│
├── 📂 ESP_Sender/                    # Firmware for Slave Node
│   ├── 📂 include/
│   │   └── main.h
│   ├── 📂 src/
│   │   ├── main.cpp                  # Entry point, RTOS tasks
│   │   ├── Sensor.cpp                # MPU6050 reading
│   │   ├── Custom_Lora.cpp           # LoRa TX/RX
│   │   ├── TDMA.cpp                  # TDMA protocol
│   │   ├── secure_data.cpp           # AES data encryption
│   │   └── secure_beacon.cpp         # Beacon handling
│   ├── 📂 test/
│   └── platformio.ini
│
├── 📂 ESP_Reciever/                  # Firmware for Master Node
│   ├── 📂 include/
│   ├── 📂 src/
│   │   ├── main.cpp
│   │   └── MQTT.cpp                  # WiFi & MQTT client
│   └── platformio.ini
│
└── 📂 software/dashboard/Example/MPU_DashBoard/    # Web Dashboard
    ├── server.js                     # Express + MQTT + MongoDB
    ├── cryptoUtils.js                # AES decryption
    ├── app.js                        # Frontend logic
    ├── index.html                    # Dashboard UI
    ├── style.css
    └── package.json
```

---

## 🚀 Installation Guide

### Step 1: Install MPU_DashBoard (Web Server)

> ⚠️ **Important:** Install and run the server first to be ready to receive data.

#### 1.1 Navigate to the directory

```bash
cd design_system_iot/software/dashboard/Example/MPU_DashBoard
```

#### 1.2 Install dependencies

```bash
npm install
```

#### 1.3 Start MongoDB

Open a **new terminal** and run:

```bash
mongod
```

> 📝 Keep this terminal open. MongoDB needs to run continuously during operation.

#### 1.4 Start the Web Server

Open **another terminal** and run:

```bash
node server.js
```

**Expected output:**

```
✅ MongoDB connected: mongodb://localhost:27017/bridge_shm
🚀 Server running on http://0.0.0.0:3019
```

#### 1.5 Verify Dashboard

Open your browser and navigate to:

```
http://localhost:3019
```

Or if accessing from another machine on the same LAN:

```
http://<HOST_IP>:3019
```

✅ Dashboard is ready to receive data.

---

### Step 2: Install ESP_Receiver (Master Node)

#### 2.1 Configure WiFi and MQTT

Open file `ESP_Reciever/src/MQTT.cpp` and edit:

```cpp
// ==== Config ====
static const char* WIFI_SSID = "YOUR_WIFI_NAME";        // ← Change this
static const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";    // ← Change this
static const char* MQTT_HOST = "broker.hivemq.com";     // Or another broker
static const uint16_t MQTT_PORT = 1883;
```

#### 2.2 Build and Upload

```bash
cd ESP_Reciever
```

```bash
pio run --target upload
```

#### 2.3 Verify Connection

```bash
pio device monitor --baud 115200
```

**Expected output:**

```
WiFi connected
MQTT connected
```

✅ ESP_Receiver is ready to receive data from Senders.

---

### Step 3: Install ESP_Sender (Slave Node)

#### 3.1 Get Chip ID

Each ESP32 has a **unique Chip ID**. You need to get this ID for configuration.

```bash
cd ESP_Sender
```

```bash
pio run --target upload
```

```bash
pio device monitor --baud 115200
```

**Sample output:**

```
ESP32 Chip ID: ABCD12345678
Slave Node ID: 0
```

> 📝 Write down the Chip ID (e.g., `ABCD12345678`)

#### 3.2 Configure Node ID

Open file `ESP_Sender/src/main.cpp`, find **lines 94-113** and add your Chip ID:

```cpp
Serial.printf("ESP32 Chip ID: %llX\n", chipid);

if (chipid == 0x34B7C9EA6BE8ULL)
{
    u8SlaveNodeID = 1;
}
else if (chipid == 0xABCD12345678ULL)     // ← Add your Chip ID
{
    u8SlaveNodeID = 2;                     // ← Assign Node ID (1, 2, 3, ...)
}
else
{
    u8SlaveNodeID = 0;                     // Unknown node
}
```

> ⚠️ **Note:** Each Sender must have a **different Node ID**.

#### 3.3 Upload again

```bash
pio run --target upload
```

#### 3.4 Verify configuration

```bash
pio device monitor --baud 115200
```

**Expected output:**

```
ESP32 Chip ID: ABCD12345678
Slave Node ID: 2
```

> ✅ Node ID other than 0 means configuration is successful.

#### 3.5 Add more Senders (optional)

Repeat steps 3.1 → 3.4 for each ESP_Sender. Remember to assign **different Node IDs**.

---

## ✅ System Verification

### Checklist

- [ ] MongoDB is running (`mongod`)
- [ ] Web server is running (`node server.js`)
- [ ] ESP_Receiver is connected to WiFi and MQTT
- [ ] ESP_Sender is sending data (Node ID ≠ 0)

### View Dashboard

Navigate to `http://localhost:3019` and observe the sensor data charts.

### Check MongoDB

```bash
mongosh
```

```javascript
use bridge_shm
db.samples.find().sort({time: -1}).limit(5)
```

**Sample output:**

```json
{
  "_id": ObjectId("..."),
  "time": ISODate("2024-01-15T10:30:00Z"),
  "area": "1",
  "name_node": "N01",
  "ax": 0.023,
  "ay": -0.015,
  "az": 0.981,
  "gx": 1.2,
  "gy": -0.8,
  "gz": 0.5
}
```

---

## ⚙️ Advanced Configuration

### Change MQTT Broker

**ESP_Receiver (`MQTT.cpp`):**

```cpp
static const char* MQTT_HOST = "test.mosquitto.org";
static const uint16_t MQTT_PORT = 1883;
```

**Server (`server.js`):**

```javascript
const MQTT_URL = 'mqtt://test.mosquitto.org:1883';
```

### Change Area ID

**ESP_Receiver (`MQTT.cpp`):**

```cpp
#define AREA_ID  2    // Change from 1 to 2
```

### Change sensor reading frequency

**ESP_Sender (`Sensor.cpp`):**

```cpp
const TickType_t periodTicks = pdMS_TO_TICKS(10);  // 10ms = 100Hz
// Change to pdMS_TO_TICKS(20) for 50Hz
```

### Change Server Port

```bash
PORT=8080 node server.js
```

### Use PM2 (background process)

```bash
npm install -g pm2
```

```bash
pm2 start server.js --name "mpu-dashboard"
```

```bash
pm2 status
```

```bash
pm2 logs mpu-dashboard
```

---

## ❌ Troubleshooting

### 🔴 Dashboard not displaying data

| Cause | Solution |
|-------|----------|
| MongoDB not running | Run `mongod` in a separate terminal |
| Server not running | Run `node server.js` |
| ESP_Receiver not connected to WiFi | Check SSID/password in `MQTT.cpp` |
| MQTT broker not working | Try another broker: `test.mosquitto.org` |

### 🔴 ESP_Sender shows Node ID = 0

| Cause | Solution |
|-------|----------|
| Chip ID not added | Add Chip ID to `main.cpp` (lines 94-113) |
| Wrong Chip ID format | Ensure format: `0xCHIP_IDULL` |

### 🔴 Error "EADDRINUSE: address already in use"

Port is already in use. Change port or kill the process:

```bash
PORT=3020 node server.js
```

### 🔴 Error "Cannot find module"

Dependencies not installed:

```bash
npm install
```

### 🔴 LoRa not receiving data

| Check | Action |
|-------|--------|
| Wiring | Check NSS, DIO0, RESET, SPI pins |
| Antenna | Ensure antenna is attached |
| Distance | Try placing closer |
| Frequency | Both must be on 433MHz |

### 🔴 MPU6050 not detected

```
MPU6050 not found
```

| Check | Action |
|-------|--------|
| I2C connection | Check SDA (GPIO 21), SCL (GPIO 22) |
| Power | Ensure 3.3V supply |
| I2C address | Default is 0x68 |

---

## 📡 API Reference

### MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `bridge/{area}/cipher` | Receiver → Server | Encrypted data |
| `bridge/{area}/{nodeId}/imu` | Receiver → Server | IMU data (JSON) |

### JSON Payload Format

```json
{
  "ts": 1705312200000,
  "areaId": 1,
  "nodeId": "N01",
  "ax": 0.023,
  "ay": -0.015,
  "az": 0.981,
  "gx": 1.2,
  "gy": -0.8,
  "gz": 0.5
}
```

### MongoDB Schema

```javascript
{
  time: Date,           // Receive timestamp
  area: String,         // Area ID
  name_node: String,    // "N01", "N02", ...
  ax: Number,           // Acceleration X (g)
  ay: Number,           // Acceleration Y (g)
  az: Number,           // Acceleration Z (g)
  gx: Number,           // Gyro X (°/s)
  gy: Number,           // Gyro Y (°/s)
  gz: Number            // Gyro Z (°/s)
}
```

---

## 🤝 Contributing

All contributions are welcome! Please:

1. Fork the repository
2. Create a new branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.

---

## 📞 Contact

- **Author:** HuyLQ
- **Website:** [lqhuy.is-a.dev](lqhuy.is-a.dev)
- **Repository:** [bitbucket.org/lqhuy125/design_system_iot](https://bitbucket.org/lqhuy125/design_system_iot)

---

<!-- <p align="center">
  Made with ❤️ for Bridge Structural Health Monitoring
</p> -->
