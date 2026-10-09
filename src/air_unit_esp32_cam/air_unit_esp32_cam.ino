/*
  Air Unit Firmware (Mounted on Drone)
  Hardware: AI-Thinker ESP32-CAM (OV2640) + LoRa Ra-02 (SX1278 433MHz)
  Connections: 
    - Hardware Serial (U0R GPIO 3, U0T GPIO 1) connected to F405 FC UART at 19200 baud.
    - Ra-02 LoRa connected over SPI on Header 1 (SCK 13, MISO 15, MOSI 14, SS 2, RST 12).
    - Ra-02 3.3V power strictly sourced from ESP32-CAM 3V3 output pin.
    - Flash LED on GPIO 4 (PWM brightness controlled via Web UI).

  Dual-Server & Dual-Core Architecture:
    - Core 0: Low-latency MAVLink UART <-> LoRa 433 MHz RF bridge (FreeRTOS pinned task).
    - Core 1: 
        * Port 80 (camera_httpd): Dedicated to UI & real-time controls (0ms latency, never blocks).
        * Port 81 (stream_httpd): Dedicated continuous MJPEG video stream worker.
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
const char* AP_SSID = "BuddyThisAintFreeWifi";
const char* AP_PASS = "notfree123";

// Runtime Camera & Flash States
int currentFlashDuty  = 0;
int currentFramesize  = FRAMESIZE_QVGA; // 5 = 320x240 (Fastest, low latency)
int currentQuality    = 12;             // 10-63 scale
int currentBrightness = 0;
int currentContrast   = 0;
int currentSaturation = 0;
int currentVflip      = 0;
int currentHmirror    = 0;
int currentEffect     = 0;
volatile bool loraReady = false;
volatile uint32_t fcByteCount = 0;
volatile uint32_t loraPktCount = 0;

httpd_handle_t camera_httpd = NULL; // Port 80 (UI & Real-time Controls)
httpd_handle_t stream_httpd = NULL; // Port 81 (Dedicated Stream Worker)
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
    // 0. Auto-reconnect if Ra-02 was powered on late or reattached
    if (!loraReady) {
      static unsigned long lastRetry = 0;
      if (millis() - lastRetry > 2000) {
        lastRetry = millis();
        if (LORA_RST != -1) {
          digitalWrite(LORA_RST, LOW);
          delay(10);
          digitalWrite(LORA_RST, HIGH);
          delay(10);
        }
        if (LoRa.begin(433E6)) {
          loraReady = true;
          LoRa.setSpreadingFactor(7);
          LoRa.setSignalBandwidth(250E3);
          LoRa.setCodingRate4(5);
          LoRa.setSyncWord(0x12);
          LoRa.setTxPower(20);
        }
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    // 1. Read MAVLink bytes from F405 flight controller -> Transmit over LoRa
    size_t bytesAvail = Serial.available();
    if (bytesAvail > 0) {
      size_t toRead = (bytesAvail > sizeof(serialBuf)) ? sizeof(serialBuf) : bytesAvail;
      Serial.readBytes(serialBuf, toRead);
      fcByteCount += toRead;
      
      LoRa.beginPacket();
      LoRa.write(serialBuf, toRead);
      LoRa.endPacket();
      loraPktCount++;
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

// =================== TACTICAL FPV COCKPIT UI ===================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
  <title>Personal UFO | FPV Cockpit</title>
  <style>
    :root {
      --bg: #070a12;
      --card-bg: rgba(15, 23, 42, 0.75);
      --card-border: rgba(30, 41, 59, 0.8);
      --accent: #00f0ff;
      --accent-glow: rgba(0, 240, 255, 0.35);
      --accent-subtle: rgba(0, 240, 255, 0.12);
      --amber: #ffb703;
      --amber-glow: rgba(255, 183, 3, 0.3);
      --red: #ff3366;
      --green: #00ff9d;
      --green-glow: rgba(0, 255, 157, 0.3);
      --text: #f1f5f9;
      --text-muted: #8492a6;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "SF Pro Display", monospace; -webkit-tap-highlight-color: transparent; }
    body { background: var(--bg); color: var(--text); padding: 12px; min-height: 100vh; overflow-x: hidden; }

    /* Top Bar */
    .top-bar { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; margin-bottom: 12px; gap: 8px; border-bottom: 1px solid var(--card-border); padding-bottom: 10px; }
    .brand { display: flex; align-items: center; gap: 8px; }
    .brand-icon { width: 10px; height: 10px; background: var(--accent); border-radius: 2px; box-shadow: 0 0 10px var(--accent); }
    .title { font-size: 14px; font-weight: 800; letter-spacing: 1.5px; color: var(--text); text-transform: uppercase; }
    .title span { color: var(--accent); }

    .telemetry-badges { display: flex; gap: 6px; flex-wrap: wrap; }
    .badge { font-size: 10px; font-weight: 700; padding: 4px 8px; border-radius: 4px; background: rgba(30, 41, 59, 0.6); border: 1px solid var(--card-border); color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; display: flex; align-items: center; gap: 5px; }
    .badge.online { background: rgba(0, 255, 157, 0.1); border-color: rgba(0, 255, 157, 0.3); color: var(--green); }
    .badge.radio { background: rgba(0, 240, 255, 0.1); border-color: rgba(0, 240, 255, 0.3); color: var(--accent); }
    .pulse-dot { width: 6px; height: 6px; border-radius: 50%; background: var(--green); box-shadow: 0 0 8px var(--green); animation: pulse 1.2s infinite; }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.25; } 100% { opacity: 1; } }

    /* Grid Layout */
    .layout-grid { display: grid; grid-template-columns: 1fr; gap: 14px; max-width: 1300px; margin: 0 auto; }
    @media(min-width: 900px) {
      .layout-grid { grid-template-columns: 1.8fr 1.2fr; align-items: start; }
    }

    /* Video Viewport */
    .viewport-card { background: #000; border: 1px solid var(--card-border); border-radius: 10px; overflow: hidden; position: relative; box-shadow: 0 8px 30px rgba(0,0,0,0.8); }
    .stream-frame { position: relative; width: 100%; min-height: 280px; display: flex; justify-content: center; align-items: center; background: #020408; overflow: hidden; transition: min-height 0.3s ease; }
    .stream-frame img { width: 100%; height: auto; display: block; object-fit: contain; transform-origin: center center; transition: transform 0.25s cubic-bezier(0.16, 1, 0.3, 1); }

    /* Tactical HUD Overlays */
    .hud-layer { position: absolute; inset: 0; pointer-events: none; z-index: 10; padding: 10px; display: flex; flex-direction: column; justify-content: space-between; }
    .hud-top { display: flex; justify-content: space-between; align-items: center; }
    .hud-tag { background: rgba(2, 6, 23, 0.8); backdrop-filter: blur(6px); border: 1px solid rgba(255,255,255,0.12); padding: 4px 8px; border-radius: 4px; font-size: 10px; font-weight: 700; color: #fff; letter-spacing: 0.5px; display: flex; align-items: center; gap: 6px; }
    .rec-flag { color: var(--red); display: flex; align-items: center; gap: 4px; }
    .rec-flag::before { content: ""; display: inline-block; width: 6px; height: 6px; background: var(--red); border-radius: 50%; animation: pulse 1s infinite; }

    /* Crosshairs */
    .reticle { position: absolute; inset: 0; display: none; pointer-events: none; }
    .reticle.visible { display: block; }
    .reticle-center { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); width: 44px; height: 44px; border: 1px solid rgba(0, 240, 255, 0.4); border-radius: 50%; }
    .reticle-center::before, .reticle-center::after { content: ""; position: absolute; background: var(--accent); }
    .reticle-center::before { top: 50%; left: -8px; right: -8px; height: 1px; transform: translateY(-50%); }
    .reticle-center::after { left: 50%; top: -8px; bottom: -8px; width: 1px; transform: translateX(-50%); }

    /* HUD Bottom Controls */
    .hud-bottom-bar { display: flex; justify-content: space-between; align-items: center; pointer-events: auto; background: rgba(10, 15, 29, 0.85); backdrop-filter: blur(8px); padding: 8px 12px; border-top: 1px solid var(--card-border); gap: 6px; flex-wrap: wrap; }
    .action-btn { background: rgba(30, 41, 59, 0.8); border: 1px solid var(--card-border); color: #fff; padding: 6px 12px; border-radius: 6px; font-size: 11px; font-weight: 700; text-transform: uppercase; cursor: pointer; display: flex; align-items: center; gap: 6px; transition: all 0.15s ease; }
    .action-btn:hover { border-color: var(--accent); color: var(--accent); box-shadow: 0 0 10px var(--accent-subtle); }
    .action-btn.active { background: var(--accent); color: #000; border-color: var(--accent); box-shadow: 0 0 12px var(--accent-glow); }

    /* Control Deck Panels */
    .control-deck { display: flex; flex-direction: column; gap: 12px; }
    .panel { background: var(--card-bg); backdrop-filter: blur(12px); border: 1px solid var(--card-border); border-radius: 10px; padding: 14px; box-shadow: 0 4px 20px rgba(0,0,0,0.4); }
    .panel-header { font-size: 11px; font-weight: 800; color: var(--accent); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid rgba(255,255,255,0.06); padding-bottom: 6px; }

    /* Flashlight Panel */
    .flash-quick-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px; margin-bottom: 10px; }
    .flash-pill { background: rgba(30, 41, 59, 0.6); border: 1px solid var(--card-border); color: var(--text-muted); padding: 6px 4px; border-radius: 6px; font-size: 10px; font-weight: 700; text-align: center; cursor: pointer; text-transform: uppercase; transition: all 0.15s; }
    .flash-pill:hover, .flash-pill.active { background: var(--amber-glow); border-color: var(--amber); color: var(--amber); }

    /* Rows and Sliders */
    .control-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; font-size: 12px; }
    .control-label { color: var(--text-muted); font-weight: 600; text-transform: uppercase; font-size: 11px; }
    .control-val { font-weight: 800; color: var(--accent); font-family: monospace; }

    input[type=range] { -webkit-appearance: none; width: 100%; background: #1e293b; height: 6px; border-radius: 3px; outline: none; margin: 4px 0 12px 0; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 18px; height: 18px; border-radius: 50%; background: var(--accent); cursor: pointer; border: 2px solid #000; box-shadow: 0 0 8px var(--accent); transition: transform 0.1s; }
    input[type=range]::-webkit-slider-thumb:hover { transform: scale(1.15); }

    /* Resolution Selector Buttons */
    .res-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px; margin-bottom: 12px; }
    .res-btn { background: rgba(30, 41, 59, 0.6); border: 1px solid var(--card-border); color: var(--text-muted); padding: 7px 4px; border-radius: 6px; font-size: 10px; font-weight: 700; text-align: center; cursor: pointer; transition: all 0.15s; }
    .res-btn:hover { border-color: var(--accent); color: #fff; }
    .res-btn.active { background: var(--accent); color: #000; border-color: var(--accent); box-shadow: 0 0 12px var(--accent-glow); }

    /* Rotation & Invert Controls */
    .rot-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px; margin-bottom: 10px; }
    .rot-btn { background: rgba(30, 41, 59, 0.6); border: 1px solid var(--card-border); color: var(--text-muted); padding: 7px 4px; border-radius: 6px; font-size: 10px; font-weight: 700; text-align: center; cursor: pointer; transition: all 0.15s; text-transform: uppercase; }
    .rot-btn:hover { border-color: var(--accent); color: #fff; }
    .rot-btn.active { background: var(--accent); color: #000; border-color: var(--accent); box-shadow: 0 0 12px var(--accent-glow); }

    .toggle-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
    .toggle-card { background: rgba(30, 41, 59, 0.4); border: 1px solid var(--card-border); padding: 8px 10px; border-radius: 6px; display: flex; justify-content: space-between; align-items: center; cursor: pointer; transition: border-color 0.15s; }
    .toggle-card:hover { border-color: var(--accent); }
    .toggle-card.active { border-color: var(--accent); background: var(--accent-subtle); }
    .toggle-title { font-size: 11px; font-weight: 700; text-transform: uppercase; color: var(--text); }

    /* Dropdowns */
    select.select-field { width: 100%; background: rgba(30, 41, 59, 0.8); border: 1px solid var(--card-border); color: #fff; padding: 7px 10px; border-radius: 6px; font-size: 11px; font-weight: 700; outline: none; margin-bottom: 10px; }

    /* Toast Notification */
    .toast-container { position: fixed; bottom: 16px; left: 16px; z-index: 100; pointer-events: none; }
    .toast { background: rgba(15, 23, 42, 0.95); border: 1px solid var(--accent); color: var(--accent); padding: 6px 14px; border-radius: 6px; font-size: 11px; font-weight: 800; letter-spacing: 0.5px; text-transform: uppercase; box-shadow: 0 4px 15px rgba(0,0,0,0.6); opacity: 0; transform: translateY(10px); transition: all 0.2s cubic-bezier(0.16, 1, 0.3, 1); }
    .toast.show { opacity: 1; transform: translateY(0); }
  </style>
</head>
<body>
  <div class="top-bar">
    <div class="brand">
      <div class="brand-icon"></div>
      <div class="title">PERSONAL UFO <span>FPV COCKPIT</span></div>
    </div>
    <div class="telemetry-badges">
      <div class="badge online"><span class="pulse-dot"></span>Stream :81</div>
      <div class="badge radio" id="loraBadge">LoRa 433 MHz [Core 0]</div>
      <div class="badge" id="fcBadge">FC Telemetry: 0 B</div>
    </div>
  </div>

  <div class="layout-grid">
    <!-- Viewport Container -->
    <div class="viewport-card">
      <div class="stream-frame" id="streamFrame">
        <div class="hud-layer">
          <div class="hud-top">
            <div class="hud-tag rec-flag">LIVE STREAM</div>
            <div class="hud-tag" id="hudResBadge">QVGA 320x240</div>
          </div>
          <div></div>
        </div>
        <div class="reticle" id="hudReticle">
          <div class="reticle-center"></div>
        </div>
        <img id="streamImg" alt="Connecting to Port 81 Dedicated Stream...">
      </div>

      <div class="hud-bottom-bar">
        <div style="display: flex; gap: 6px; flex-wrap: wrap;">
          <button class="action-btn" onclick="captureSnapshot()">Snapshot</button>
          <button class="action-btn" id="btnQuickRotate" onclick="cycleQuickRotate()">Rot +90°</button>
          <button class="action-btn" id="btnReticle" onclick="toggleReticle()">Reticle</button>
          <button class="action-btn" onclick="reconnectStream()">Reconnect</button>
        </div>
        <button class="action-btn" onclick="toggleFullscreen()">Expand</button>
      </div>
    </div>

    <!-- Live Controls Deck -->
    <div class="control-deck">
      <!-- Searchlight / Flash LED (GPIO 4) -->
      <div class="panel">
        <div class="panel-header">
          <span>Searchlight / Flash LED (GPIO 4)</span>
          <span id="flashStateLabel" style="color:var(--amber); font-family:monospace;">OFF</span>
        </div>
        <div class="flash-quick-grid">
          <button class="flash-pill active" id="btnFlashOff" onclick="setFlashPreset(0)">OFF</button>
          <button class="flash-pill" id="btnFlash25" onclick="setFlashPreset(64)">25%</button>
          <button class="flash-pill" id="btnFlash50" onclick="setFlashPreset(128)">50%</button>
          <button class="flash-pill" id="btnFlash100" onclick="setFlashPreset(255)">MAX</button>
        </div>
        <div class="control-row">
          <span class="control-label">Continuous Dimmer</span>
          <span class="control-val" id="flashVal">0%</span>
        </div>
        <input type="range" id="flashSlider" min="0" max="255" value="0" oninput="onFlashSliderInput(this.value)">
      </div>

      <!-- Mount Orientation & Live 90° Rotation -->
      <div class="panel">
        <div class="panel-header">
          <span>Camera Tilt & Mount Orientation</span>
          <span id="rotDegLabel" style="color:var(--accent); font-family:monospace;">0°</span>
        </div>
        
        <!-- Live GPU-Accelerated 90° Rotation Buttons -->
        <div class="control-row">
          <span class="control-label">Live View Rotation</span>
        </div>
        <div class="rot-grid">
          <button class="rot-btn active" id="rot0" onclick="setLiveRotation(0)">0° Normal</button>
          <button class="rot-btn" id="rot90" onclick="setLiveRotation(90)">90° CW</button>
          <button class="rot-btn" id="rot180" onclick="setLiveRotation(180)">180° Invert</button>
          <button class="rot-btn" id="rot270" onclick="setLiveRotation(270)">270° CW</button>
        </div>

        <!-- Hardware Sensor Inversions -->
        <div class="toggle-grid" style="margin-top:6px;">
          <div class="toggle-card" id="cardVflip" onclick="toggleInvert('vflip')">
            <span class="toggle-title">V-Flip (Inverted)</span>
          </div>
          <div class="toggle-card" id="cardHmirror" onclick="toggleInvert('hmirror')">
            <span class="toggle-title">H-Mirror</span>
          </div>
        </div>
      </div>

      <!-- Resolution & Compression -->
      <div class="panel">
        <div class="panel-header">
          <span>Optical Resolution (Port 81)</span>
          <span style="color:var(--text-muted); font-size:9px;">INSTANT REBIND</span>
        </div>
        <div class="res-grid">
          <button class="res-btn active" id="res5" onclick="setResolution(5, 'QVGA (320x240)')">QVGA 30FPS</button>
          <button class="res-btn" id="res6" onclick="setResolution(6, 'CIF (400x296)')">CIF 25FPS</button>
          <button class="res-btn" id="res8" onclick="setResolution(8, 'VGA (640x480)')">VGA 20FPS</button>
          <button class="res-btn" id="res9" onclick="setResolution(9, 'SVGA (800x600)')">SVGA</button>
          <button class="res-btn" id="res11" onclick="setResolution(11, 'HD (1280x720)')">HD 720P</button>
          <button class="res-btn" id="res13" onclick="setResolution(13, 'UXGA (1600x1200)')">2MP STILL</button>
        </div>

        <div class="control-row">
          <span class="control-label">JPEG Compression</span>
          <span class="control-val" id="qualityVal">12</span>
        </div>
        <input type="range" min="10" max="40" value="12" id="qualitySlider" oninput="sendLiveControl('quality', this.value, 'qualityVal')">
      </div>

      <!-- Sensor Image Adjustments -->
      <div class="panel">
        <div class="panel-header">Sensor Image Tuning</div>
        <div class="control-row">
          <span class="control-label">Brightness</span>
          <span class="control-val" id="brightVal">0</span>
        </div>
        <input type="range" min="-2" max="2" value="0" oninput="sendLiveControl('brightness', this.value, 'brightVal')">

        <div class="control-row">
          <span class="control-label">Contrast</span>
          <span class="control-val" id="contrastVal">0</span>
        </div>
        <input type="range" min="-2" max="2" value="0" oninput="sendLiveControl('contrast', this.value, 'contrastVal')">

        <div class="control-row">
          <span class="control-label">Special Effect</span>
        </div>
        <select class="select-field" onchange="sendLiveControl('special_effect', this.value)">
          <option value="0" selected>Normal</option>
          <option value="1">Negative</option>
          <option value="2">Grayscale</option>
          <option value="3">Red Tint</option>
          <option value="4">Green Tint</option>
          <option value="5">Blue Tint</option>
          <option value="6">Sepia</option>
        </select>
      </div>
    </div>
  </div>

  <div class="toast-container">
    <div class="toast" id="toastBox">Ready</div>
  </div>

  <script>
    // Direct stream connection to dedicated Port 81
    const streamPort = 81;
    const streamUrl = window.location.protocol + '//' + window.location.hostname + ':' + streamPort + '/stream';
    const streamImg = document.getElementById('streamImg');
    const streamFrame = document.getElementById('streamFrame');
    
    // Connect stream on page boot
    streamImg.src = streamUrl;

    let vflipState = 0;
    let hmirrorState = 0;
    let reticleState = false;
    let toastTimer = null;
    let currentRotation = 0;

    // Toast Alert Helper
    function showToast(msg) {
      const box = document.getElementById('toastBox');
      box.innerText = msg;
      box.classList.add('show');
      clearTimeout(toastTimer);
      toastTimer = setTimeout(() => box.classList.remove('show'), 1400);
    }

    // High-performance Non-blocking Control Dispatcher
    function sendLiveControl(varName, value, labelId) {
      if (labelId) document.getElementById(labelId).innerText = value;
      
      // Send to Port 80 dedicated controls endpoint
      fetch(`/control?var=${varName}&val=${value}`)
        .then(() => showToast(`${varName.toUpperCase()}: ${value}`))
        .catch(err => console.error("Control dispatch failed", err));
    }

    // Live GPU-Accelerated 90-Degree Rotation Engine
    function setLiveRotation(deg) {
      currentRotation = parseInt(deg);
      
      // Highlight active button
      document.querySelectorAll('.rot-btn').forEach(btn => btn.classList.remove('active'));
      const activeBtn = document.getElementById('rot' + currentRotation);
      if (activeBtn) activeBtn.classList.add('active');

      document.getElementById('rotDegLabel').innerText = currentRotation + '°';

      // Apply GPU transform
      if (currentRotation === 90 || currentRotation === 270) {
        streamImg.style.transform = `rotate(${currentRotation}deg) scale(0.75)`;
        streamFrame.style.minHeight = '370px'; // Give portrait headroom
      } else if (currentRotation === 180) {
        streamImg.style.transform = `rotate(180deg) scale(1)`;
        streamFrame.style.minHeight = '280px';
      } else {
        streamImg.style.transform = `rotate(0deg) scale(1)`;
        streamFrame.style.minHeight = '280px';
      }

      // Persist to browser localStorage so refresh remembers tilt
      localStorage.setItem('ufo_cam_rot', currentRotation);
      showToast(`ROTATED: ${currentRotation}°`);
    }

    function cycleQuickRotate() {
      const nextRot = (currentRotation + 90) % 360;
      setLiveRotation(nextRot);
    }

    // Load saved rotation on startup
    const savedRot = localStorage.getItem('ufo_cam_rot');
    if (savedRot !== null) {
      setLiveRotation(parseInt(savedRot));
    }

    // Flash LED Controls
    function setFlashPreset(duty) {
      document.getElementById('flashSlider').value = duty;
      onFlashSliderInput(duty);
    }

    function onFlashSliderInput(duty) {
      const percent = Math.round((duty / 255) * 100);
      document.getElementById('flashVal').innerText = percent + '%';
      
      const label = document.getElementById('flashStateLabel');
      label.innerText = duty > 0 ? `${percent}% ON` : 'OFF';
      label.style.color = duty > 0 ? 'var(--green)' : 'var(--amber)';

      // Highlight preset buttons
      document.getElementById('btnFlashOff').classList.toggle('active', duty == 0);
      document.getElementById('btnFlash25').classList.toggle('active', duty > 0 && duty <= 64);
      document.getElementById('btnFlash50').classList.toggle('active', duty > 64 && duty <= 150);
      document.getElementById('btnFlash100').classList.toggle('active', duty > 150);

      sendLiveControl('flash', duty);
    }

    // Resolution Switcher with Dedicated Stream Reconnect
    function setResolution(resVal, resLabel) {
      document.querySelectorAll('.res-btn').forEach(btn => btn.classList.remove('active'));
      const activeBtn = document.getElementById('res' + resVal);
      if (activeBtn) activeBtn.classList.add('active');

      document.getElementById('hudResBadge').innerText = resLabel;
      showToast(`SETTING ${resLabel}`);

      fetch(`/control?var=framesize&val=${resVal}`)
        .then(() => {
          setTimeout(reconnectStream, 350);
        });
    }

    // Hardware Invert Toggles
    function toggleInvert(type) {
      if (type === 'vflip') {
        vflipState = vflipState ? 0 : 1;
        document.getElementById('cardVflip').classList.toggle('active', vflipState);
        sendLiveControl('vflip', vflipState);
      } else if (type === 'hmirror') {
        hmirrorState = hmirrorState ? 0 : 1;
        document.getElementById('cardHmirror').classList.toggle('active', hmirrorState);
        sendLiveControl('hmirror', hmirrorState);
      }
    }

    // Reticle HUD Crosshair
    function toggleReticle() {
      reticleState = !reticleState;
      document.getElementById('hudReticle').classList.toggle('visible', reticleState);
      document.getElementById('btnReticle').classList.toggle('active', reticleState);
      showToast(reticleState ? 'RETICLE ACTIVE' : 'RETICLE HIDDEN');
    }

    // Stream Reconnection Helper
    function reconnectStream() {
      streamImg.src = streamUrl + '?t=' + Date.now();
      showToast('STREAM RECONNECTED');
    }

    // High-res Snapshot Downloader
    function captureSnapshot() {
      window.open('/capture?t=' + Date.now(), '_blank');
      showToast('SNAPSHOT CAPTURED');
    }

    // Native Fullscreen Toggle
    function toggleFullscreen() {
      const frame = document.getElementById('streamFrame');
      if (!document.fullscreenElement) {
        frame.requestFullscreen().catch(err => alert(err.message));
      } else {
        document.exitFullscreen();
      }
    }

    // Dynamic telemetry status polling
    function pollStatus() {
      fetch('/status')
        .then(r => r.json())
        .then(data => {
          const badge = document.getElementById('loraBadge');
          if (badge) {
            badge.innerText = data.lora ? 'LoRa [Armed]' : 'LoRa [Standby]';
            badge.className = data.lora ? 'badge online' : 'badge radio';
          }
          const fcBadge = document.getElementById('fcBadge');
          if (fcBadge) {
            if (data.fcBytes > 1024) {
              fcBadge.innerText = 'FC: ' + (data.fcBytes / 1024).toFixed(1) + ' KB (' + data.pkts + ' TX)';
              fcBadge.className = 'badge online';
            } else if (data.fcBytes > 0) {
              fcBadge.innerText = 'FC: ' + data.fcBytes + ' B (' + data.pkts + ' TX)';
              fcBadge.className = 'badge online';
            } else {
              fcBadge.innerText = 'FC Telemetry: 0 B';
              fcBadge.className = 'badge';
            }
          }
        })
        .catch(() => {});
    }
    setInterval(pollStatus, 2000);
    pollStatus();
  </script>
</body>
</html>
)rawliteral";

// =================== HTTP URI HANDLERS ===================
// 1. Root handler (Port 80): Serves the Web Cockpit UI
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// 2. High-speed Dedicated MJPEG Stream Handler (Port 81)
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

// 3. Single Frame Capture Handler (Port 80)
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

// 4. Real-Time Hardware & Sensor Control Handler (Port 80 - NEVER BLOCKS!)
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

// 5. System Status Handler (Port 80)
static esp_err_t status_handler(httpd_req_t *req) {
  char json[160];
  snprintf(json, sizeof(json), "{\"lora\":%s,\"fcBytes\":%u,\"pkts\":%u,\"flash\":%d,\"res\":%d}", 
           loraReady ? "true" : "false", fcByteCount, loraPktCount, currentFlashDuty, currentFramesize);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

// Start Dual-Port Web & Stream Architecture
void startCameraServer() {
  // 1. Port 80 Server: Dedicated to Cockpit UI and Instant Controls
  httpd_config_t config_ui = HTTPD_DEFAULT_CONFIG();
  config_ui.server_port = 80;
  config_ui.ctrl_port = 32768; // Primary control port
  config_ui.max_uri_handlers = 8;
  config_ui.stack_size = 4096;

  httpd_uri_t index_uri   = { .uri = "/",        .method = HTTP_GET, .handler = index_handler,   .user_ctx = NULL };
  httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL };
  httpd_uri_t control_uri = { .uri = "/control", .method = HTTP_GET, .handler = control_handler, .user_ctx = NULL };
  httpd_uri_t status_uri  = { .uri = "/status",  .method = HTTP_GET, .handler = status_handler,  .user_ctx = NULL };

  if (httpd_start(&camera_httpd, &config_ui) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &control_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
  }

  // 2. Port 81 Server: Dedicated Exclusively to the MJPEG Stream
  httpd_config_t config_stream = HTTPD_DEFAULT_CONFIG();
  config_stream.server_port = 81;
  config_stream.ctrl_port = 32769; // Distinct secondary control port (avoids socket collision)
  config_stream.max_uri_handlers = 2;
  config_stream.stack_size = 4096;

  httpd_uri_t stream_uri  = { .uri = "/stream",  .method = HTTP_GET, .handler = stream_handler,  .user_ctx = NULL };

  if (httpd_start(&stream_httpd, &config_stream) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
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

  // 2. Initialize Wi-Fi Soft AP & Dual-Port HTTP Server
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
    loraReady = true;
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
  // Loop is free because both http servers run asynchronously in their own FreeRTOS worker tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
