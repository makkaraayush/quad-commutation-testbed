/*
  Air Unit Firmware (Mounted on Drone)
  Hardware: AI-Thinker ESP32-CAM (OV2640) + LoRa Ra-02 (SX1278 433MHz)
  Connections: 
    - Hardware Serial (U0R GPIO 3, U0T GPIO 1) connected to F405 FC UART at 19200 baud.
    - Ra-02 LoRa connected over SPI using SD Card pins (SCK 14, MISO 2, MOSI 13, SS 15).
    - Ra-02 3.3V power strictly sourced from ESP32-CAM 3V3 output pin.

  Dual-Core Architecture:
    - Core 0: Low-latency MAVLink UART <-> LoRa 433 MHz RF bridge (FreeRTOS pinned task).
    - Core 1: Wi-Fi Soft AP ("DRONE_CAM") hosting live MJPEG video stream at http://192.168.4.1/stream.
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>

// =================== PIN DEFINITIONS ===================
// LoRa SPI Bus on ESP32-CAM SD card lines
#define LORA_SCK       14
#define LORA_MISO      2
#define LORA_MOSI      13
#define LORA_SS        15
#define LORA_RST       12      // Hardware Reset pin (Header 1, Pin 3 on ESP32-CAM)
#define LORA_DIO0      -1

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

WiFiServer camServer(80);
TaskHandle_t LoRaBridgeTask;

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

// Single-frame HTTP capture handler
void handleSingleFrame(WiFiClient &client) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    client.println("HTTP/1.1 500 Internal Error\r\n\r\n");
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Content-Disposition: inline; filename=capture.jpg");
  client.print("Content-Length: ");
  client.println(fb->len);
  client.println("Connection: close\r\n");
  client.write(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
}

// =================== SETUP ===================
void setup() {
  // Serial0 connects to F405 FC UART at 19200 baud
  Serial.begin(19200);

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
  
  // QVGA (320x240) balances smooth frame rates (~20-25 FPS) and minimal RAM usage
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 12; // 10-63 scale (lower is crisper quality)
  config.fb_count     = 2;

  esp_camera_init(&config);

  // 2. Initialize Wi-Fi Soft AP
  WiFi.softAP(AP_SSID, AP_PASS);
  camServer.begin();

  // 3. Hardware pulse reset for Ra-02 (SX1278) to guarantee clean transceiver state
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

// =================== CORE 1: WEB STREAMING LOOP ===================
void loop() {
  WiFiClient client = camServer.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        String req = client.readStringUntil('\r');
        client.readStringUntil('\n');

        // Continuous MJPEG Stream: http://192.168.4.1/stream
        if (req.indexOf("/stream") >= 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n");
          
          while (client.connected()) {
            camera_fb_t * fb = esp_camera_fb_get();
            if (!fb) break;

            client.println("--frame");
            client.println("Content-Type: image/jpeg");
            client.print("Content-Length: ");
            client.println(fb->len);
            client.println("\r\n");
            client.write(fb->buf, fb->len);
            client.println();
            esp_camera_fb_return(fb);

            vTaskDelay(40 / portTICK_PERIOD_MS); // ~25 FPS pacing
          }
          break;
        } else {
          // Fallback single-frame capture: http://192.168.4.1/
          handleSingleFrame(client);
          break;
        }
      }
    }
    client.stop();
  }
  vTaskDelay(10 / portTICK_PERIOD_MS);
}
