# CYD Sand Simulation

## Project Overview
Sand physics simulation for ESP32-2432S028 (Cheap Yellow Display) with MPU-9250/6500/9255 accelerometer.

## Hardware
- **Board**: ESP32-2432S028 (CYD) - ESP32 with 320x240 ILI9341 TFT + XPT2046 resistive touchscreen
- **Accelerometer**: MPU-9250/6500/9255 on I2C (SDA=GPIO22, SCL=GPIO27, CN1 connector)
- **Display SPI (HSPI)**: DC=2, MISO=12, MOSI=13, SCK=14, CS=15, BL=21
- **Touch SPI (VSPI)**: CLK=25, MOSI=32, CS=33, IRQ=36, MISO=39

## Build
- Framework: Arduino via PlatformIO
- Libraries: TFT_eSPI (build flags config), XPT2046_Touchscreen (Paul Stoffregen)
- Compile: `platformio run`
- Upload: `platformio run --target upload`

## Architecture
- Single-file project: `src/main.cpp`
- Grid-based sand simulation (160x120 cells, 2x2 pixel each)
- 8-directional gravity from accelerometer
- Incremental rendering (only changed cells redrawn)
