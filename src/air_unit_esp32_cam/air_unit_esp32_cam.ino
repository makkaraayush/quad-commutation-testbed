/*
  Air Unit Firmware (Mounted on Drone)
  Hardware: AI-Thinker ESP32-CAM (OV2640) + LoRa Ra-02 (SX1278 433MHz)
  Connections: 
    - Hardware Serial (U0R GPIO 3, U0T GPIO 1) connected to F405 FC UART at 19200 baud.
    - Ra-02 LoRa connected over SPI on Header 1 (SCK 13, MISO 15, MOSI 14, SS 2, RST 12).
    - Ra-02 3.3V power strictly sourced from ESP32-CAM 3V3 output pin.
    - Flash LED on GPIO 4 (PWM brightness controlled via Web UI).

  Dual-Core Architecture:
    - Core 0: Low-latency MAVLink UART <-> LoRa 433 MHz RF bridge (FreeRTOS pinned task).
    - Core 1: esp_http_server hosting live FPV MJPEG video stream & tactical cockpit UI at http://192.168.4.1/.
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>
#include "esp_http_server.h"

// =================== PIN DEFINITIONS ===================
// LoRa SPI Bus (Straight 4-wire physical ribbon: SCK, MISO, MOSI, NSS + RST)
#define LORA_SCK       13      // SPI Clock (Header 1, Pin 4)
#define LORA_MISO      15      // SPI Master In Slave Out (Header 1, Pin 5)
#define LORA_MOSI      14      // SPI Master Out Slave In (Header 1, Pin 6)
#define LORA_SS        2       // SPI Chip Select (Header 1, Pin 7)
#define LORA_RST       12      // Hardware Reset pin (Header 1, Pin 3)
#define LORA_DIO0      -1

// Onboard High-Power Flash LED
#define FLASH_LED_PIN  4

// AI-Thinker OV2640 Camera Pin Map
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Wi-Fi Access Point Credentials
const char* AP_SSID = "DRONE_CAM";
const char* AP_PASS = "12345678";

// Runtime Camera & Flash States
int currentFlashDuty = 0;
int currentFramesize = FRAMESIZE_QVGA; // 5 = 320x240 (Fastest, low latency)
int currentQuality   = 12;             // 10-63 scale (lower = crisper)
int currentBrightness = 0;
int currentContrast   = 0;
int currentSaturation = 0;
int currentVflip      = 0;
int currentHmirror    = 0;
int currentEffect     = 0;

httpd_handle_t camera_httpd = NULL;
TaskHandle_t LoRaBridgeTask;

// =================== FLASH LED PWM CONTROL ===================
void setFlashBrightness(int duty) {
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;
  currentFlashDuty = duty;

  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(FLASH_LED_PIN, duty);
  #else
    ledcWrite(7, duty);
  #endif
}

void initFlash() {
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW); // Start with flash completely OFF
  
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(FLASH_LED_PIN, 5000, 8);
    ledcWrite(FLASH_LED_PIN, 0);
  #else
    ledcSetup(7, 5000, 8);
    ledcAttachPin(FLASH_LED_PIN, 7);
    ledcWrite(7, 0);
  #endif
}

// =================== CORE 0: MAVLINK <-> LORA BRIDGE ===================
void loraBridgeLoop(void * pvParameters) {
  uint8_t serialBuf[128];
  
  for(;;) {
    // 1. Read MAVLink bytes from F405 flight controller -> Transmit over LoRa
    size_t bytesAvail = Serial.available();
    if (bytesAvail > 0) {
      size_t toRead = (bytesAvail > sizeof(serialBuf)) ? sizeof(serialBuf) : bytesAvail;
      Serial.readBytes(serialBuf, toRead);
      
      LoRa.beginPacket();
      LoRa.write(serialBuf, toRead);
      LoRa.endPacket();
    }

    // 2. Read incoming LoRa packets (Ground Station commands) -> Forward to F405
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
      while (LoRa.available()) {
        Serial.write((uint8_t)LoRa.read());
      }
    }
    
    // Yield to FreeRTOS scheduler to prevent watchdog resets
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
}

// =================== EMBEDDED TACTICAL COCKPIT UI ===================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
  <title>Drone Air Unit | FPV Cockpit</title>
  <style>
    :root {
      --bg: #0a0e17;
      --card: #111827;
      --border: #1f293d;
      --text: #e2e8f0;
      --text-dim: #94a3b8;
      --accent: #00e5ff;
      --accent-glow: rgba(0, 229, 255, 0.25);
      --amber: #f59e0b;
      --red: #ef4444;
      --green: #10b981;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace; }
    body { background: var(--bg); color: var(--text); padding: 12px; min-height: 100vh; }
    .header { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; margin-bottom: 12px; gap: 8px; border-bottom: 1px solid var(--border); padding-bottom: 10px; }
    .title { font-size: 16px; font-weight: 700; letter-spacing: 1px; color: var(--accent); text-transform: uppercase; }
    .status-bar { display: flex; gap: 6px; flex-wrap: wrap; }
    .badge { font-size: 11px; padding: 3px 8px; border-radius: 4px; background: #1e293b; color: var(--text-dim); border: 1px solid var(--border); font-weight: 600; text-transform: uppercase; }
    .badge.active { background: rgba(16, 185, 129, 0.15); color: var(--green); border-color: rgba(16, 185, 129, 0.4); }
    .badge.radio { background: rgba(0, 229, 255, 0.15); color: var(--accent); border-color: rgba(0, 229, 255, 0.4); }

    .main-grid { display: grid; grid-template-columns: 1fr; gap: 14px; max-width: 1200px; margin: 0 auto; }
    @media(min-width: 860px) {
      .main-grid { grid-template-columns: 2fr 1fr; }
    }

    /* Video Viewport */
    .stream-container { position: relative; background: #000; border: 1px solid var(--border); border-radius: 8px; overflow: hidden; display: flex; justify-content: center; align-items: center; min-height: 240px; box-shadow: 0 4px 20px rgba(0,0,0,0.6); }
    .stream-container img { width: 100%; height: auto; display: block; object-fit: contain; }
    
    .hud-overlay { position: absolute; top: 8px; left: 8px; right: 8px; display: flex; justify-content: space-between; pointer-events: none; z-index: 10; }
    .hud-tag { background: rgba(0, 0, 0, 0.65); backdrop-filter: blur(4px); padding: 3px 8px; border-radius: 4px; font-size: 11px; font-weight: bold; border: 1px solid rgba(255,255,255,0.1); }
    .rec-indicator { display: inline-block; width: 8px; height: 8px; background: var(--red); border-radius: 50%; margin-right: 5px; animation: pulse 1s infinite; }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.3; } 100% { opacity: 1; } }

    .hud-controls { position: absolute; bottom: 8px; right: 8px; display: flex; gap: 6px; z-index: 10; }
    .hud-btn { background: rgba(17, 24, 39, 0.85); backdrop-filter: blur(4px); border: 1px solid var(--border); color: #fff; padding: 5px 10px; border-radius: 4px; font-size: 11px; cursor: pointer; font-weight: 600; text-transform: uppercase; }
    .hud-btn:hover { border-color: var(--accent); color: var(--accent); }

    /* Controls Panel */
    .controls-deck { display: flex; flex-direction: column; gap: 12px; }
    .panel { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 12px; }
    .panel-header { font-size: 12px; font-weight: 700; color: var(--accent); text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 10px; display: flex; justify-content: space-between; align-items: center; }

    .control-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; font-size: 13px; }
    .control-row:last-child { margin-bottom: 0; }
    .control-label { color: var(--text-dim); }
    .control-val { font-weight: 600; color: var(--accent); min-width: 32px; text-align: right; }

    input[type=range] { -webkit-appearance: none; width: 100%; background: #1e293b; height: 6px; border-radius: 3px; outline: none; margin: 6px 0; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 16px; height: 16px; border-radius: 50%; background: var(--accent); cursor: pointer; border: 2px solid #000; }

    select, button.action-btn { width: 100%; background: #1e293b; border: 1px solid var(--border); color: var(--text); padding: 8px 10px; border-radius: 4px; font-size: 12px; outline: none; }
    select:focus { border-color: var(--accent); }
    
    .btn-group { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-top: 8px; }
    .btn-toggle { background: #1e293b; border: 1px solid var(--border); color: var(--text-dim); padding: 8px; border-radius: 4px; font-size: 11px; font-weight: 600; cursor: pointer; text-align: center; text-transform: uppercase; }
    .btn-toggle.active { background: var(--accent-glow); color: var(--accent); border-color: var(--accent); }

    .switch-row { display: flex; justify-content: space-between; align-items: center; }
    .switch { position: relative; display: inline-block; width: 38px; height: 20px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #1e293b; transition: .2s; border-radius: 20px; border: 1px solid var(--border); }
    .slider:before { position: absolute; content: ""; height: 14px; width: 14px; left: 2px; bottom: 2px; background-color: #94a3b8; transition: .2s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--accent); border-color: var(--accent); }
    input:checked + .slider:before { transform: translateX(18px); background-color: #000; }
  </style>
</head>
<body>
  <div class="header">
    <div class="title">Drone Air Unit Cockpit</div>
    <div class="status-bar">
      <div class="badge active"><span class="rec-indicator"></span>LIVE FPV</div>
      <div class="badge radio">433 MHz LoRa [Core 0]</div>
      <div class="badge">19200 Baud FC</div>
    </div>
  </div>

  <div class="main-grid">
    <!-- Main Video Stream -->
    <div>
      <div class="stream-container" id="streamContainer">
        <div class="hud-overlay">
          <div class="hud-tag"><span class="rec-indicator"></span>LIVE STREAM</div>
          <div class="hud-tag" id="hudResolution">320x240 (QVGA)</div>
        </div>
        <img id="stream" src="/stream" alt="Connecting to FPV Stream...">
        <div class="hud-controls">
          <button class="hud-btn" onclick="captureSnapshot()">Snapshot</button>
          <button class="hud-btn" onclick="reloadStream()">Refresh</button>
          <button class="hud-btn" onclick="toggleFullscreen()">Expand</button>
        </div>
      </div>
    </div>

    <!-- Controls Deck -->
    <div class="controls-deck">
      <!-- Flashlight / Night Ops -->
      <div class="panel">
        <div class="panel-header">
          <span>Searchlight / Flash LED (GPIO 4)</span>
          <label class="switch">
            <input type="checkbox" id="flashToggle" onchange="toggleFlash(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <span class="control-label">Brightness</span>
          <span class="control-val" id="flashVal">0%</span>
        </div>
        <input type="range" id="flashSlider" min="0" max="255" value="0" oninput="updateFlash(this.value)">
      </div>

      <!-- Resolution & Stream Quality -->
      <div class="panel">
        <div class="panel-header">Stream Optics & Resolution</div>
        <div class="control-row">
          <span class="control-label">Resolution</span>
        </div>
        <select id="framesizeSelect" onchange="setResolution(this.value)">
          <option value="5" selected>QVGA (320x240) - Low Latency FPV</option>
          <option value="6">CIF (400x296) - Balanced Flight</option>
          <option value="8">VGA (640x480) - High Res</option>
          <option value="9">SVGA (800x600) - Detailed</option>
          <option value="11">HD (1280x720) - HD Stills</option>
        </select>

        <div class="control-row" style="margin-top:12px;">
          <span class="control-label">JPEG Compression</span>
          <span class="control-val" id="qualityVal">12</span>
        </div>
        <input type="range" id="qualitySlider" min="10" max="40" value="12" onchange="sendCmd('quality', this.value, 'qualityVal')">
      </div>

      <!-- Optical Image Adjustments -->
      <div class="panel">
        <div class="panel-header">Sensor Image Tuning</div>
        <div class="control-row">
          <span class="control-label">Brightness</span>
          <span class="control-val" id="brightVal">0</span>
        </div>
        <input type="range" min="-2" max="2" value="0" onchange="sendCmd('brightness', this.value, 'brightVal')">

        <div class="control-row">
          <span class="control-label">Contrast</span>
          <span class="control-val" id="contrastVal">0</span>
        </div>
        <input type="range" min="-2" max="2" value="0" onchange="sendCmd('contrast', this.value, 'contrastVal')">

        <div class="control-row">
          <span class="control-label">Special Effect</span>
        </div>
        <select onchange="sendCmd('special_effect', this.value)">
          <option value="0" selected>Normal</option>
          <option value="1">Negative</option>
          <option value="2">Grayscale</option>
          <option value="3">Red Tint</option>
          <option value="4">Green Tint</option>
          <option value="5">Blue Tint</option>
          <option value="6">Sepia</option>
        </select>
      </div>

      <!-- Drone Inverted Mount Adjustments -->
      <div class="panel">
        <div class="panel-header">Frame Mounting Orientation</div>
        <div class="btn-group">
          <button class="btn-toggle" id="btnVflip" onclick="toggleVflip()">Flip Vertical</button>
          <button class="btn-toggle" id="btnHmirror" onclick="toggleHmirror()">Mirror Horiz</button>
        </div>
      </div>
    </div>
  </div>

  <script>
    let vflipState = 0;
    let hmirrorState = 0;

    function sendCmd(variable, value, labelId) {
      if(labelId) document.getElementById(labelId).innerText = value;
      fetch(`/control?var=${variable}&val=${value}`).catch(e => console.log(e));
    }

    function toggleFlash(enabled) {
      const slider = document.getElementById('flashSlider');
      let duty = enabled ? (slider.value > 0 ? slider.value : 128) : 0;
      if (enabled && slider.value == 0) slider.value = 128;
      updateFlash(duty);
    }

    function updateFlash(val) {
      const percent = Math.round((val / 255) * 100);
      document.getElementById('flashVal').innerText = percent + '%';
      document.getElementById('flashToggle').checked = (val > 0);
      sendCmd('flash', val);
    }

    function setResolution(val) {
      const resMap = {
        '5': '320x240 (QVGA)',
        '6': '400x296 (CIF)',
        '8': '640x480 (VGA)',
        '9': '800x600 (SVGA)',
        '11': '1280x720 (HD)'
      };
      document.getElementById('hudResolution').innerText = resMap[val] || 'Custom';
      fetch(`/control?var=framesize&val=${val}`).then(() => {
        setTimeout(reloadStream, 400);
      });
    }

    function toggleVflip() {
      vflipState = vflipState ? 0 : 1;
      document.getElementById('btnVflip').classList.toggle('active', vflipState);
      sendCmd('vflip', vflipState);
    }

    function toggleHmirror() {
      hmirrorState = hmirrorState ? 0 : 1;
      document.getElementById('btnHmirror').classList.toggle('active', hmirrorState);
      sendCmd('hmirror', hmirrorState);
    }

    function captureSnapshot() {
      window.open('/capture?t=' + Date.now(), '_blank');
    }

    function reloadStream() {
      const img = document.getElementById('stream');
      img.src = '/stream?t=' + Date.now();
    }

    function toggleFullscreen() {
      const elem = document.getElementById('streamContainer');
      if (!document.fullscreenElement) {
        elem.requestFullscreen().catch(err => alert(err.message));
      } else {
        document.exitFullscreen();
      }
    }
  </script>
</body>
</html>
)rawliteral";

// =================== HTTP URI HANDLERS ===================
// 1. Root handler: Serves the Web Cockpit UI
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// 2. High-speed MJPEG Stream Handler
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }
    _jpg_buf_len = fb->len;
    _jpg_buf = fb->buf;

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) break;
    vTaskDelay(20 / portTICK_PERIOD_MS); // ~25-35 FPS pacing
  }
  return res;
}

// 3. Single Frame Capture Handler
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// 4. Hardware & Sensor Control Handler
static esp_err_t control_handler(httpd_req_t *req) {
  char buf[64];
  size_t buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1 && buf_len <= sizeof(buf)) {
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      char var[16] = {0};
      char val_str[16] = {0};
      if (httpd_query_key_value(buf, "var", var, sizeof(var)) == ESP_OK &&
          httpd_query_key_value(buf, "val", val_str, sizeof(val_str)) == ESP_OK) {
        int val = atoi(val_str);
        sensor_t * s = esp_camera_sensor_get();
        int res = 0;

        if (!strcmp(var, "framesize")) {
          if (s->pixformat == PIXFORMAT_JPEG) {
            res = s->set_framesize(s, (framesize_t)val);
            if (res == 0) currentFramesize = val;
          }
        } else if (!strcmp(var, "quality")) {
          res = s->set_quality(s, val);
          if (res == 0) currentQuality = val;
        } else if (!strcmp(var, "contrast")) {
          res = s->set_contrast(s, val);
          if (res == 0) currentContrast = val;
        } else if (!strcmp(var, "brightness")) {
          res = s->set_brightness(s, val);
          if (res == 0) currentBrightness = val;
        } else if (!strcmp(var, "saturation")) {
          res = s->set_saturation(s, val);
          if (res == 0) currentSaturation = val;
        } else if (!strcmp(var, "special_effect")) {
          res = s->set_special_effect(s, val);
          if (res == 0) currentEffect = val;
        } else if (!strcmp(var, "vflip")) {
          res = s->set_vflip(s, val);
          if (res == 0) currentVflip = val;
        } else if (!strcmp(var, "hmirror")) {
          res = s->set_hmirror(s, val);
          if (res == 0) currentHmirror = val;
        } else if (!strcmp(var, "flash")) {
          setFlashBrightness(val);
          res = 0;
        } else {
          res = -1;
        }

        if (res < 0) return httpd_resp_send_500(req);
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, "OK", 2);
      }
    }
  }
  return httpd_resp_send_404(req);
}

// Start HTTP Server
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;
  config.max_uri_handlers = 8;
  config.stack_size = 4096;

  httpd_uri_t index_uri   = { .uri = "/",        .method = HTTP_GET, .handler = index_handler,   .user_ctx = NULL };
  httpd_uri_t stream_uri  = { .uri = "/stream",  .method = HTTP_GET, .handler = stream_handler,  .user_ctx = NULL };
  httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL };
  httpd_uri_t control_uri = { .uri = "/control", .method = HTTP_GET, .handler = control_handler, .user_ctx = NULL };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &control_uri);
  }
}

// =================== SETUP ===================
void setup() {
  // Serial0 connects to F405 FC UART at 19200 baud
  Serial.begin(19200);

  // Initialize Flashlight PWM (GPIO 4)
  initFlash();

  // 1. Initialize OV2640 Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // QVGA (320x240) balances high FPS (~25-30 FPS) and low latency for FPV
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 12; // 10-63 scale (lower is crisper quality)
  config.fb_count     = 2;

  esp_camera_init(&config);

  // 2. Initialize Wi-Fi Soft AP & Non-blocking HTTP Server
  WiFi.softAP(AP_SSID, AP_PASS);
  startCameraServer();

  // 3. Hardware pulse reset for Ra-02 (SX1278)
  if (LORA_RST != -1) {
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(10);
  }

  // Initialize LoRa SPI Bus
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  
  if (LoRa.begin(433E6)) {
    LoRa.setSpreadingFactor(7);           // Fast throughput for MAVLink
    LoRa.setSignalBandwidth(250E3);        // 250 kHz bandwidth
    LoRa.setCodingRate4(5);               // 4/5 coding rate
    LoRa.setSyncWord(0x12);                // Private drone sync word
    LoRa.setTxPower(20);                   // +20dBm (100mW max RF output)
  }

  // 4. Pin LoRa bridge to Core 0 so video serving on Core 1 never drops packets
  xTaskCreatePinnedToCore(
    loraBridgeLoop,
    "LoRaBridge",
    4096,
    NULL,
    2,
    &LoRaBridgeTask,
    0
  );
}

void loop() {
  // Loop is free because esp_http_server runs asynchronously in its own task on Core 1
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
