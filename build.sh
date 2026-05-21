#!/bin/bash
set -e

FQBN="esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=dio"
PORT="${1:-/dev/ttyUSB0}"

echo "==> Compiling..."
arduino-cli compile --fqbn "$FQBN" .

echo "==> Uploading to $PORT..."
arduino-cli upload --fqbn "$FQBN" --port "$PORT" .

echo "==> Monitoring $PORT (Ctrl+] to quit)..."
arduino-cli monitor --port "$PORT" --config baudrate=115200
