#!/bin/bash
# setup-rpi.sh — установка и запуск MJPEG-ретранслятора на Raspberry Pi (Ubuntu)
# Перенеси папку проекта на Pi целиком (scp/rsync) и запусти из неё:
#   scp -r relay/ docker-compose.yml pi-16.local:~/esp32-cam-cctv/
#   ssh pi-16.local 'cd ~/esp32-cam-cctv && bash setup-rpi.sh'

set -e

if [ -z "${CAM_HOST}" ]; then
    echo "Ошибка: передай CAM_HOST (IP камеры)"
    echo "Пример: CAM_HOST=192.168.0.111 bash setup-rpi.sh"
    exit 1
fi
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "==> Камера: ${CAM_HOST}"
echo "==> Папка проекта: ${PROJECT_DIR}"

# ── 1. Docker ──────────────────────────────────────────────────────
if ! command -v docker &>/dev/null; then
    echo "==> Ставим Docker..."
    curl -fsSL https://get.docker.com | sh
fi

# добавляем пользователя в группу docker
if ! groups | grep -q docker; then
    sudo usermod -aG docker "$USER"
    echo "==> Добавлен в группу docker. Перезайди или выполни: newgrp docker"
fi

# ── 2. .env с IP камеры ─────────────────────────────────────────────
if [ ! -f .env ] || ! grep -q "CAM_HOST" .env; then
    echo "CAM_HOST=${CAM_HOST}" > .env
    echo "==> .env создан (CAM_HOST=${CAM_HOST})"
fi

# ── 3. Запуск ───────────────────────────────────────────────────────
echo "==> Запускаем relay..."
CAM_HOST="${CAM_HOST}" docker compose up -d --build

sleep 2
docker compose logs relay | tail -5

echo
echo "============================================="
echo "  Открой http://$(hostname -I 2>/dev/null | awk '{print $1}').local:8080"
echo "  или  http://$(hostname -I 2>/dev/null | awk '{print $1}'):8080"
echo "============================================="
