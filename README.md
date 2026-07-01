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

🚀 Step-by-Step Execution & Deployment Flow
This guide walks you through building the initial firmware, flashing it to the board, and then successfully performing your first wireless OTA update.

Phase 1: The Initial Flash (V1 — LED Only)
Our initial firmware (Version 1.x.x) is configured to only blink the onboard LED, proving that the base architecture and Wi-Fi connection work.

1. Activate the nRF Connect SDK Open your terminal and ensure the nRF Connect environment is active (required for west).

2. Run the Build Script

powershell


Remove-Item -Recurse -Force build
.\build_ota.bat
What this script does: It compiles the Zephyr OS, your application code, and the MCUboot bootloader. It then hashes the binary (SHA-256 and CRC-32), updates manifest.json, and automatically pushes the .bin and manifest to your GitHub server repository.

3. Flash the Board via USB (One-Time Only)

powershell


west flash --recover
This physically programs the MCUboot bootloader and the V1 application into the nRF5340 chip. You will never need to plug in the USB cable to flash again.

4. V1 Observation Open a serial terminal (115200 baud). You will see:

MCUboot confirming the image.
Wi-Fi connecting successfully.
An OTA check stating: Firmware is up to date.
The green LED on the board blinking every 500ms.
Phase 2: The Wireless OTA Update (V2 — LED + Sensors)
Now we will wirelessly upgrade the board to V2, which activates the BMP280 and TMP117 sensors.

1. Enable Sensor Logic Open src/user_applications.c. Remove the comments around the sensor code so that the I2C sensors are initialized and read during the main loop.

2. Bump the Firmware Version Open the VERSION file in the root directory and increment the version (so the board knows there is an update):



VERSION_MAJOR = 2
VERSION_MINOR = 0
PATCHLEVEL = 0
3. Build and Push the Update

powershell


.\build_ota.bat
Note: Do NOT run west flash! The script has already pushed the new V2 binary to your GitHub server.

4. Wait for Cache Wait 1 to 2 minutes for GitHub's raw.githubusercontent.com servers to refresh their cache.

5. V2 Observation (The OTA update) Press the RESET button on your board. Watch the serial terminal:

The board boots V1 and connects to Wi-Fi.
It polls GitHub and detects that 2.0.0 is greater than the current version.
You will see a live progress bar as it streams the .bin file in 1KB chunks.
Upon completion, it verifies the CRC-32 and SHA-256 hashes.
The board automatically reboots. MCUboot swaps the images, and you will now see V2 booting, the LED blinking, and live BMP280/TMP117 sensor data appearing in the logs!
⚠️ Things to Keep in Mind
GitHub Server Cache: Always wait ~2 minutes after running build_ota.bat before resetting your board. If you reset too fast, the board fetches the old manifest.json and misses the update.
I2C Pin Connections: Ensure your sensors are wired precisely to SCL = P1.02 and SDA = P1.03 with common ground and 3.3V power. The firmware auto-detects BMP280 addresses 0x76 and 0x77 based on your SDO pin.
Clean Builds: Always delete the build/ folder (or run Remove-Item -Recurse -Force build) if you change Kconfig options (prj.conf), add new .c files, or modify device tree overlays.
🛠️ Using This Framework for Your Own Applications
This firmware uses a modular architecture. You can easily replace the LED and Sensor logic with your own custom applications without breaking the OTA engine.

Keep the Core Intact: Do not modify ota_http.c, wifi_mgr.c, or main.c unless you are changing underlying networking parameters.
Write Your Module: Create a new .c and .h file in src/ (e.g., Motor_Control.c).
Register It: Add your new .c file to CMakeLists.txt.
Hook It Up: Include your header in src/user_applications.c. Call your initialization function inside user_applications_init() and your execution logic inside the user_applications_run() while-loop.
Deploy: Bump the VERSION file, run build_ota.bat, wait 2 minutes, and reset your fleet!

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
