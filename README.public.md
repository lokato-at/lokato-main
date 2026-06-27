# Lokato — Hardware & Project Hub

> Real-time room-occupancy system for after-school childcare facilities.
> This repo is the **hardware + project-overview hub**. The server-side
> platform (Laravel backend + Vue frontend) lives in
> [`lokato-platform`](https://github.com/lokato-at/lokato-platform).

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-E7352C)](https://www.espressif.com/en/products/socs/esp32)

---

## What this is

Lokato is an interactive room display: each door of an after-school
facility has an RFID scanner that recognises children's trackers as they
walk through. A Raspberry Pi server aggregates the events and serves a
live dashboard to staff plus per-room tablets for the children.

This repository contains:

| Directory | Contents |
|---|---|
| [`esp32_rfid_mqtt_prod/`](esp32_rfid_mqtt_prod/) | Production firmware: Arduino sketch + R200 UHF-RFID driver |
| [`tablet_mockup/`](tablet_mockup/) | Static HTML/JS mock-up of the per-room tablet view |
| [`scripts/`](scripts/) | Raspberry Pi initial-setup script & tutorial |
| [`docs/diagrams/`](docs/diagrams/) | Architecture diagrams, ER diagrams (Mermaid + PNG) |

The matching server-side code is in **[`lokato-platform`](https://github.com/lokato-at/lokato-platform)**.

---

## Hardware

| Component | Notes |
|---|---|
| ESP32 (classic) | One per scanner, one per room door |
| R200 UHF-RFID reader + antenna | 860–960 MHz, passive EPC Gen2 tags |
| RFID tracker tag | One per child, attached to clothing / school bag |
| Raspberry Pi 4 or 5 (64-bit Pi OS) | Hosts MQTT broker + Laravel + MariaDB + nginx |
| Tablets (any HTML5 browser) | One per room door, plus a shared entrance tablet for parents |

### Firmware setup

```cpp
// esp32_rfid_mqtt_prod/esp32_rfid_mqtt_prod.ino
static const char* DEVICE_KEY    = "<unique-per-scanner>";   // ← set me
static const char* WIFI_SSID     = "<your-network>";          // ← set me
static const char* WIFI_PASSWORD = "<network-password>";      // ← set me
static const char* MQTT_HOST     = "<pi-ip-on-lan>";          // ← set me
```

> **Do not commit these values.** Copy
> [`esp32_rfid_mqtt_prod/README_PROD.md`](esp32_rfid_mqtt_prod/README_PROD.md)
> as a guide, change the placeholders to your real WiFi/MQTT setup, and
> keep the modified `.ino` out of git (it is in `.gitignore` patterns).

---

## Quick links

- **Server / web app**: [lokato-platform](https://github.com/lokato-at/lokato-platform)
- **Architecture diagrams**: [`docs/diagrams/`](docs/diagrams/)
- **Pi setup tutorial**: [`scripts/rpi-setup-tutorial.md`](scripts/rpi-setup-tutorial.md)
- **License**: [MIT](LICENSE)

---

## Acknowledgements

- Student project at **FH OÖ, Campus Hagenberg** (winter semester 2025 /
  summer semester 2026)
- Project supervision: **Wolfgang Hochleitner**, **Volker Christian**
- Maintainers: Edina Abazovic, Selina Catic
- Originally developed in partnership with a real-world after-school
  childcare facility; deployed and tested on-site
