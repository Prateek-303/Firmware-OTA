<div align="center">

# OTA_Firmware

**Over-The-Air Firmware Update System for nRF7002 DK (nRF5340)**

[![Project Repo](https://img.shields.io/badge/Repo-Firmware--OTA-informational?logo=github)](https://github.com/Prateek-303/Firmware-OTA)
[![Server Repo](https://img.shields.io/badge/Server-nRF53--OTA--server-lightgrey?logo=github)](https://github.com/Prateek-303/nRF53-OTA-server)
[![SDK](https://img.shields.io/badge/nRF%20Connect%20SDK-v3.2.4-orange)](https://developer.nordicsemi.com)
[![Board](https://img.shields.io/badge/Board-nRF7002%20DK-green)](https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK)
[![Zephyr](https://img.shields.io/badge/Zephyr%20OS-v4.2.99-blueviolet)](https://zephyrproject.org)
[![MCUboot](https://img.shields.io/badge/Bootloader-MCUboot-blue)](https://mcuboot.com)

*by Prateek Baraiya*

</div>

---

## 📌 About

This is the **firmware source code** repository for an OTA (Over-The-Air) update system built on the Nordic nRF5340 SoC with the nRF7002 Wi-Fi companion chip. The firmware connects to Wi-Fi, polls a GitHub-hosted manifest file, downloads a new binary if a version update is found, verifies it cryptographically, and hands it off to MCUboot for a safe atomic swap — all automatically and wirelessly.

> The server repository (manifest + binaries) is at:
> 👉 [github.com/Prateek-303/nRF53-OTA-server](https://github.com/Prateek-303/nRF53-OTA-server)

---

## 📁 Project Structure

```
OTA_Firmware/
 ┣ src/
 ┃  ┣ main.c                  ← Entry point: boot, Wi-Fi init, OTA trigger
 ┃  ┣ wifi_mgr.c / .h         ← Wi-Fi connection and DHCP management
 ┃  ┣ ota_http.c / .h         ← OTA engine: HTTPS poll, download, verify, swap
 ┃  ┣ user_applications.c / .h← Application orchestrator (V1/V2 toggle)
 ┃  ┣ Led_Blink.c / .h        ← LED blink logic (V1)
 ┃  ┣ Sensors_OTA.c / .h      ← BMP280 + TMP117 I2C sensor logic (V2)
 ┃  └ github_certs.h          ← Hardcoded ISRG Root X1 TLS certificate
 ┣ boards/
 ┃  └ nrf7002dk_nrf5340_cpuapp.overlay  ← I2C pins + external flash config
 ┣ child_image/mcuboot.conf    ← MCUboot bootloader config
 ┣ sysbuild/mcuboot/           ← Sysbuild MCUboot overrides
 ┣ gitpush/
 ┃  ┣ push.bat                 ← Push README to server repo
 ┃  └ push_project.bat         ← Push source code to this repo
 ┣ CMakeLists.txt              ← Source file registration
 ┣ prj.conf                    ← Kconfig: Wi-Fi, TLS, memory, logging
 ┣ pm_static.yml               ← Flash partition layout
 ┣ VERSION                     ← Semantic version file
 ┣ build_ota.bat               ← Build + auto-push to server repo
 └ update_manifest.py          ← Computes SHA-256 + CRC-32, writes manifest
```

---

## ⚙️ How It Works

```
Board boots
    │
    ├─▶ Confirms MCUboot image (rollback protection)
    ├─▶ Registers TLS certificate
    ├─▶ Connects to Wi-Fi
    └─▶ Spawns OTA background thread (every 24h)
              │
              ├─▶ HTTPS GET manifest.json from GitHub
              ├─▶ Compares version → if newer:
              │       ├─▶ Checks MAC authorization
              │       ├─▶ Streams .bin in 1KB chunks
              │       ├─▶ Verifies CRC-32 live during download
              │       ├─▶ Verifies SHA-256 after download
              │       └─▶ Requests MCUboot image swap → reboots
              └─▶ If same version → sleeps 24h
```

---

## 🔢 Version Strategy

| Version | Active Features | How to Enable |
|---------|---------------|---------------|
| `1.x.x` | LED blink only | Sensor calls commented out in `user_applications.c` |
| `2.x.x` | LED + BMP280 + TMP117 | Uncomment sensor include, init, and read calls |

Edit `VERSION` file and bump `PATCHLEVEL` for every new OTA push.

---

## 🔌 Hardware — I2C Wiring (V2 Sensors)

| nRF7002 DK Pin | Sensor Pin |
|----------------|-----------|
| `P1.02` | SCL |
| `P1.03` | SDA |
| `3.3V` | VCC |
| `GND` | GND |

BMP280 I2C address: `0x76` (SDO → GND) or `0x77` (SDO → 3.3V). Firmware auto-detects both.

---

## 🚀 Build & Deploy

**Prerequisites:** nRF Connect SDK v3.2.4 activated in terminal.

```powershell
# First time — clean build
Remove-Item -Recurse -Force build
.\build_ota.bat

# First time push to GitHub
git push -u origin main

# All future pushes — just double-click:
gitpush\push_project.bat
```

`build_ota.bat` compiles the firmware, computes hashes, updates `manifest.json`, and pushes the binary to the server repo automatically.

---

## 📋 Prerequisites

| Tool | Version |
|------|---------|
| nRF Connect SDK | v3.2.4 |
| nRF Connect Toolchain | fd21892d0f |
| Python | 3.x |
| Git | Any |

---

<div align="center">
<em>by <strong>Prateek Baraiya</strong></em>
</div>