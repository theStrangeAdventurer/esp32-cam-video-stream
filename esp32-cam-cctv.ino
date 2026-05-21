#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_camera.h>

// ── WiFi ────────────────────────────────────────────────────────────
#include "config.h"

// ── Camera pins (GOOUUU ESP32-S3 CAM / OV2640) ──────────────────────
//   Совпадает с CAMERA_MODEL_ESP32S3_EYE из camera_pins.h
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM   4
#define SIOC_GPIO_NUM   5
#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM     8
#define Y3_GPIO_NUM     9
#define Y2_GPIO_NUM     11
#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM   13

// ── LED (NeoPixel на GPIO 48, backlight на GPIO 21) ──────────────────
#define LED_GPIO 21

// ── Globals ──────────────────────────────────────────────────────────
WebServer server(80);

// ── Root page (инструкция) ─────────────────────────────────────────
static const char PAGE_DIRECT[] PROGMEM = R"raw(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-CAM</title>
  <style>
    body{background:#111;color:#ccc;font:16px sans-serif;padding:24px}
    code{background:#333;padding:2px 6px;border-radius:4px}
    a{color:#4af}
  </style>
</head>
<body>
  <h2>ESP32-S3 CAM</h2>
  <p>Камера работает в режиме одного клиента.</p>
  <p>Для просмотра с нескольких устройств запустите relay:</p>
  <pre>git clone ... && cd esp32-cam-cctv
CAM_HOST=&lt;этот-IP&gt; bash setup-rpi.sh</pre>
  <p>И откройте <code>http://&lt;ip-сервера&gt;:8080</code></p>
</body>
</html>
)raw";

// ── MJPEG stream handler ────────────────────────────────────────────
static void handleStream() {
  if (!server.hasHeader("x-multiclient-stream")) {
    server.send(403, "text/plain", "403 Forbidden — use relay on port 8080 (see /)");
    return;
  }

  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      delay(10);
      continue;
    }

    client.println("--frame");
    client.print("Content-Type: image/jpeg\r\nContent-Length: ");
    client.println(fb->len);
    client.println();
    client.write(fb->buf, fb->len);
    client.println();

    esp_camera_fb_return(fb);

    // feed watchdog on single-core
    delay(1);
  }
}


void handleRoot() {
  server.send_P(200, "text/html", PAGE_DIRECT);
}

// ── Camera init ─────────────────────────────────────────────────────
static bool initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = Y2_GPIO_NUM;
  cfg.pin_d1       = Y3_GPIO_NUM;
  cfg.pin_d2       = Y4_GPIO_NUM;
  cfg.pin_d3       = Y5_GPIO_NUM;
  cfg.pin_d4       = Y6_GPIO_NUM;
  cfg.pin_d5       = Y7_GPIO_NUM;
  cfg.pin_d6       = Y8_GPIO_NUM;
  cfg.pin_d7       = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size   = FRAMESIZE_VGA;
  cfg.jpeg_quality = 12;
  cfg.fb_count     = 1;
  cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  cfg.fb_location  = CAMERA_FB_IN_PSRAM;

  if (psramFound()) {
    Serial.printf("PSRAM found: %d MB\n", ESP.getPsramSize() / 0x100000);
    cfg.jpeg_quality = 10;
    cfg.fb_count     = 2;
    cfg.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    Serial.println("PSRAM not found — using DRAM");
    cfg.frame_size   = FRAMESIZE_QVGA;
    cfg.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 0);
    s->set_hmirror(s, 0);
  }

  return true;
}

// ── Setup ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  // LED
  pinMode(LED_GPIO, OUTPUT);
  pinMode(2, OUTPUT); digitalWrite(2, HIGH); // встроенный красный LED off

  // WiFi
  #ifdef WIFI_IP
    IPAddress ip(WIFI_IP), gw(WIFI_GATEWAY), sn(WIFI_SUBNET);
    WiFi.config(ip, gw, sn);
  #endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi failed — restarting in 5s");
    delay(5000);
    ESP.restart();
  }

  Serial.println();
  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  Serial.println("  (http://esp32cam.local)");

  MDNS.begin("esp32cam");

  // Camera
  if (!initCamera()) {
    Serial.println("Camera failed — restarting in 5s");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Camera OK");
  digitalWrite(LED_GPIO, LOW);

  // Web server
  server.on("/",        handleRoot);
  server.on("/stream",  handleStream);

  // без этого hasHeader("x-multiclient-stream") всегда false
  const char* hdrKeys[] = {"x-multiclient-stream"};
  server.collectHeaders(hdrKeys, 1);

  server.begin();
  Serial.println("HTTP server started on :80");
}

// ── Loop ─────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  delay(1);
}
