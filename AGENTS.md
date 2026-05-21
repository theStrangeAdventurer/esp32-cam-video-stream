# AGENTS.md

## Project

ESP32-S3 CAM video streaming to local network. Board has two USB Type-C connectors and a built-in camera.

## Toolchain

- **Framework:** Arduino (arduino-cli or Arduino IDE)
- **Board:** ESP32-S3 (select `esp32:esp32:esp32s3` or board-specific FQBN)
- **Board package:** `esp32` by Espressif (install via board manager URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`)

## Build & Flash (arduino-cli)

```sh
# GOOUUU ESP32-S3 CAM требует OPI PSRAM
arduino-cli compile --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=dio .
arduino-cli upload   --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=dio --port /dev/ttyUSB0 .
arduino-cli monitor  --port /dev/ttyUSB0 --config baudrate=115200
```

Or use `./build.sh [port]`.

## Connection

- Подключай **UART/TTL порт** (CP2102/CH340) — он всегда определяется как COM-порт и нужен для прошивки.
- OTG-порт (нативный USB CDC) даст последовательный порт только если в настройках платы включено «USB CDC On Boot».
- На Linux порт отображается как `/dev/ttyUSB0` или `/dev/ttyACM0`.

## First-time setup

```sh
cp config.h.example config.h   # затем впиши WIFI_SSID / WIFI_PASS
```

## Quirks

- Enable "USB CDC On Boot" in board options for serial output over native USB.
- The `esp32-camera` library ships with the ESP32 board package — no separate install needed.
- `config.h` in `.gitignore` — not committed to avoid leaking WiFi credentials.
- If camera fails to init («camera not detected»), try the alternative pinout commented in the sketch.

## Conventions

- README and comments may be in Russian.
- Main sketch file should match the directory name (`esp32-cam-cctv.ino`).
