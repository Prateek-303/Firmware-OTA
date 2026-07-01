cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ota_firmware_project)

target_sources(app PRIVATE
    src/main.c
    src/wifi_mgr.c
    src/ota_http.c
    src/user_applications.c
    src/Led_Blink.c
    src/Sensors_OTA.c
)