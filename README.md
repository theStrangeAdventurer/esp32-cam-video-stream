# ESP32-S3 CAM CCTV

Стриминг видео со встроенной камеры ESP32-S3 в локальную сеть с поддержкой
нескольких зрителей одновременно.

## Как это работает

```
                     Локальная сеть
┌──────────┐                              ┌──────────┐
│ ESP32-CAM│──:80/stream───────────────→  │  relay   │
│   .ino   │   (одно TCP-соединение)      │  .js     │
└──────────┘                              │  Docker  │
                                          │  :8080   │
                                          └────┬─────┘
                                               │
                          ┌─────────────────────┼──────────────
                          │                     │              │
                       Браузер               Браузер         ...
```

**ESP32-CAM** отдаёт MJPEG-стрим ровно одному клиенту.  
**relay.js** (Node.js в Docker-контейнере) — единственный клиент камеры. Он принимает
MJPEG-поток и раздаёт его всем подключившимся браузерам без перекодирования.
Задержка — ноль, потому что кадры ретранслятся как есть.

### Защита от прямого подключения

ESP32 проверяет HTTP-заголовок `x-multiclient-stream`. Только relay.js его отправляет.
Браузеры — нет. Поэтому:

| Кто | Запрос | Ответ |
|-----|--------|-------|
| Браузер → `http://cam.local/` | `GET /` | HTML: «запусти relay» |
| Браузер → `http://cam.local/stream` | без заголовка | `403 Forbidden` |
| relay.js → `http://cam.local/stream` | `x-multiclient-stream: 1` | MJPEG-стрим |

Никакой зритель не сможет «отжать» стрим у relay, открыв камеру напрямую в браузере.

## Файлы в проекте

| Файл | Зачем |
|------|-------|
| `esp32-cam-cctv.ino` | Прошивка камеры. WiFi, MJPEG-сервер, проверка заголовка |
| `config.h` | WiFi SSID/пароль (в `.gitignore`) |
| `config.h.example` | Шаблон для `config.h` |
| `build.sh` | Компиляция и прошивка через `arduino-cli` |
| `AGENTS.md` | Инструкции для AI-ассистентов и сборщика |
| `relay/relay.js` | Node.js сервер: один TCP-клиент к камере → N HTTP-клиентов |
| `relay/Dockerfile` | Сборка Docker-образа для relay |
| `docker-compose.yml` | Запуск relay в контейнере |
| `setup-rpi.sh` | Скрипт установки и запуска на Raspberry Pi (Ubuntu) |

## Установка

### 0. config.h

```sh
cp config.h.example config.h
# Впиши WIFI_SSID и WIFI_PASS своей сети
```

### 1. Прошивка ESP32

```sh
./build.sh /dev/ttyUSB0
```

После прошивки камера подключится к WiFi и будет доступна:
- `http://esp32cam.local` — страница-инструкция
- `http://<IP-камеры>` — IP берётся из serial-монитора

### 2. Запуск relay (любой Linux в той же сети)

```sh
CAM_HOST=192.168.0.111 docker compose up -d --build
```

Открыть: **`http://<ip-сервера>:8080`**

### На Raspberry Pi (Ubuntu)

Копируем проект на Pi и запускаем скрипт:

```sh
# с основного компа
scp -r relay/ docker-compose.yml setup-rpi.sh pi-16.local:~/esp32-cam-cctv/

# на Pi
ssh pi-16.local
cd ~/esp32-cam-cctv
CAM_HOST=192.168.0.111 bash setup-rpi.sh
```

Скрипт поставит Docker (если ещё нет), создаст `.env` с IP камеры, соберёт и запустит relay.

## Зависимости

### ESP32

- **Плата:** `esp32:esp32:esp32s3` (Arduino board package)
- **Библиотеки:** все встроенные (`WiFi`, `WebServer`, `ESPmDNS`, `esp_camera`)
- Никаких сторонних библиотек не требуется

### Relay

- **Docker** + **docker compose** (v2)
- Node.js — в контейнере, на хосте **не нужен**

## Примечания

- Всё работает внутри локальной сети. Никакого шифрования, авторизации, HTTPS.
  Для хобби, умного дома на коленке и CCTV в пределах своей квартиры — ОК.
  Для доступа из интернета — заворачивайте в VPN (WireGuard/Tailscale).

- ESP32 тянет только одного клиента на своём MJPEG-сервере. Без relay одновременный
  просмотр с двух устройств невозможен.

- `network_mode: host` в docker-compose нужен чтобы relay резолвил mDNS-имена
  и не требовал модулей `bridge`/`iptables` в ядре (актуально для кастомных ядер
  и Raspberry Pi с урезанными модулями).

## Если что-то не работает

```sh
# Логи relay
docker compose logs relay

# Проверить что порт слушает
ss -tlnp | grep 8080

# Проверить коннект к камере с хоста
curl -H "x-multiclient-stream: 1" http://esp32cam.local/stream -o /dev/null
```
