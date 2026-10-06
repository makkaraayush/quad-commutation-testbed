/*
  Ground Station Firmware (Mounted on FlySky FS-i6 Transmitter)
  Hardware: ESP32 DevKit + LoRa Ra-02 (433MHz) + 0.96" I2C OLED (JMD0.96D-1 SSD1306)
  Power: Dedicated 1S 3.7V 520mAh LiPo with 100k/100k divider on GPIO 34.

  Features:
  - Receives MAVLink telemetry from Drone Air Unit over 433 MHz LoRa.
  - Forwards raw MAVLink packets to Android smartphone via Bluetooth Classic SPP ("Drone_Telemetry").
    (Keeps phone 4G/5G mobile internet completely active for live satellite map tiles in QGroundControl!)
  - Forwards waypoint missions and commands from QGroundControl back to the drone over LoRa.
  - Displays live telemetry directly on the OLED: Flight Mode, Sats, Drone Battery %, TX Battery %, Altitude, Lat/Lon.
*/

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BluetoothSerial.h"

// =================== PIN DEFINITIONS ===================
// LoRa SPI Bus (Standard ESP32 VSPI)
#define LORA_SCK        18
#define LORA_MISO       19
#define LORA_MOSI       23
#define LORA_SS         5
#define LORA_RST        -1
#define LORA_DIO0       -1

// OLED I2C Bus
#define OLED_SDA        21
#define OLED_SCL        22
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDR_1   0x3C
#define SCREEN_ADDR_2   0x3D

// Transmitter Station Battery ADC (100k/100k Resistor Divider)
#define STATION_BAT_PIN 34

// =================== OBJECTS ===================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BluetoothSerial SerialBT;

// =================== TELEMETRY VARIABLES ===================
float droneBatVolts     = 0.0;
int   droneBatPct       = 0;
float droneAltitude     = 0.0;
float droneLat          = 0.0;
float droneLon          = 0.0;
int   droneSats         = 0;
char  flightMode[12]    = "DISARMED";

float stationBatVolts   = 0.0;
int   stationBatPct     = 0;

unsigned long lastTelemetryTime = 0;
unsigned long lastDisplayUpdate  = 0;
unsigned long lastBatCheckTime   = 0;
bool oledReady = false;

// =================== LIGHTWEIGHT MAVLINK PARSER ===================
// Custom zero-overhead MAVLink v1 / v2 state machine parser.
// Extracts only essential fields without requiring massive external libraries.
void parseMavlinkByte(uint8_t c) {
  static uint8_t state = 0;
  static uint8_t payloadLen = 0;
  static uint8_t msgId = 0;
  static uint8_t buf[64];
  static uint8_t bufIdx = 0;

  switch (state) {
    case 0: // Search for MAVLink frame start byte
      if (c == 0xFD) { state = 1; }      // MAVLink 2 start byte
      else if (c == 0xFE) { state = 10; } // MAVLink 1 start byte
      break;

    // --- MAVLink 2 Framing ---
    case 1: payloadLen = c; state = 2; break; // Payload length
    case 2: state = 3; break;                 // Incompatible flags
    case 3: state = 4; break;                 // Compatible flags
    case 4: state = 5; break;                 // Packet sequence
    case 5: state = 6; break;                 // System ID
    case 6: state = 7; break;                 // Component ID
    case 7: msgId = c; state = 8; break;      // Message ID Low byte
    case 8: state = 9; break;                 // Message ID Mid byte
    case 9: state = 20; bufIdx = 0; break;    // Message ID High byte -> Payload start

    // --- MAVLink 1 Framing ---
    case 10: payloadLen = c; state = 11; break; // Payload length
    case 11: state = 12; break;                 // Sequence
    case 12: state = 13; break;                 // System ID
    case 13: state = 14; break;                 // Component ID
    case 14: msgId = c; state = 20; bufIdx = 0; break; // Message ID -> Payload start

    // --- Payload Extraction ---
    case 20:
      if (bufIdx < sizeof(buf)) {
        buf[bufIdx++] = c;
      }

      if (bufIdx >= payloadLen) {
        lastTelemetryTime = millis();

        // 1. HEARTBEAT (Message #0): Decode ArduCopter Flight Mode
        if (msgId == 0 && payloadLen >= 9) {
          uint32_t customMode = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
          switch (customMode) {
            case 0:  strcpy(flightMode, "STABILIZE"); break;
            case 2:  strcpy(flightMode, "ALT_HOLD");  break;
            case 3:  strcpy(flightMode, "AUTO");      break;
            case 5:  strcpy(flightMode, "LOITER");    break;
            case 6:  strcpy(flightMode, "RTL");       break;
            case 9:  strcpy(flightMode, "LAND");      break;
            case 16: strcpy(flightMode, "POSHOLD");   break;
            case 15: strcpy(flightMode, "AUTOTUNE");  break;
            default: strcpy(flightMode, "ARMED");     break;
          }
        }
        // 2. SYS_STATUS (Message #1): Drone 3S LiPo Voltage & Percentage
        else if (msgId == 1 && payloadLen >= 31) {
          uint16_t mVolts = buf[14] | (buf[15] << 8);
          droneBatVolts = mVolts / 1000.0;
          // Approximate curve for 3S LiPo: 10.5V empty (0%) to 12.6V full (100%)
          droneBatPct = constrain((int)((droneBatVolts - 10.5) / (12.6 - 10.5) * 100.0), 0, 100);
        }
        // 3. GLOBAL_POSITION_INT (Message #33): Lat, Lon, and Relative Altitude
        else if (msgId == 33 && payloadLen >= 28) {
          int32_t lat = (int32_t)(buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24));
          int32_t lon = (int32_t)(buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24));
          int32_t relAltMm = (int32_t)(buf[16] | (buf[17] << 8) | (buf[18] << 16) | (buf[19] << 24));
          droneLat = lat / 1e7;
          droneLon = lon / 1e7;
          droneAltitude = relAltMm / 1000.0;
        }
        // 4. GPS_RAW_INT (Message #24): Satellites Visible
        else if (msgId == 24 && payloadLen >= 30) {
          droneSats = buf[29];
        }

        state = 0; // Ready for next packet
      }
      break;
  }
}

// =================== TRANSMITTER BATTERY MONITOR ===================
void updateStationBattery() {
  analogSetPinAttenuation(STATION_BAT_PIN, ADC_11db);
  uint32_t rawSum = 0;
  for (int i = 0; i < 16; i++) {
    rawSum += analogRead(STATION_BAT_PIN);
    delay(1);
  }
  float rawAvg = rawSum / 16.0;

  // 100k/100k divider factor = 2.0; 1.05 compensates for ESP32 ADC non-linearity
  stationBatVolts = (rawAvg / 4095.0) * 3.3 * 2.0 * 1.05;

  // 1S LiPo voltage curve: 3.40V empty (0%) to 4.20V full (100%)
  int pct = (int)((stationBatVolts - 3.40) / (4.20 - 3.40) * 100.0);
  stationBatPct = constrain(pct, 0, 100);
}

// =================== OLED SCREEN UPDATE ===================
void updateOLED() {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Link alive watchdog: flag lost if no packet arrived in last 3.5 seconds
  bool linkAlive = (millis() - lastTelemetryTime < 3500) && (lastTelemetryTime > 0);

  // --- ROW 1: Flight Mode & GPS Satellites ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (linkAlive) {
    display.print(flightMode);
  } else {
    display.print("NO TELEMETRY");
  }

  display.setCursor(82, 0);
  display.print("SAT:");
  display.print(linkAlive ? droneSats : 0);

  // Header Divider Line
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // --- ROW 2: Drone Battery & Transmitter Station Battery ---
  display.setCursor(0, 14);
  display.print("DRN:");
  if (linkAlive) {
    display.print(droneBatVolts, 1);
    display.print("V (");
    display.print(droneBatPct);
    display.print("%)");
  } else {
    display.print("--.-V (--%)");
  }

  display.setCursor(0, 25);
  display.print("TX :");
  display.print(stationBatVolts, 2);
  display.print("V (");
  display.print(stationBatPct);
  display.print("%)");

  // Middle Divider Line
  display.drawLine(0, 36, 128, 36, SSD1306_WHITE);

  // --- ROW 3: Relative Altitude & GPS Coordinates ---
  display.setCursor(0, 40);
  display.print("ALT: ");
  if (linkAlive) {
    display.print(droneAltitude, 1);
    display.print(" m");
  } else {
    display.print("--.- m");
  }

  display.setCursor(0, 52);
  if (linkAlive && droneLat != 0.0) {
    display.print(droneLat, 4);
    display.print(",");
    display.print(droneLon, 4);
  } else if (linkAlive) {
    display.print("ACQUIRING GPS FIX");
  } else {
    display.print("AWAITING AIR UNIT");
  }

  display.display();
}

// =================== SETUP ===================
void setup() {
  Serial.begin(115200);

  // 1. Initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR_1)) {
    oledReady = true;
  } else if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR_2)) {
    oledReady = true;
  }

  if (oledReady) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 18);
    display.println("GROUND STATION");
    display.setCursor(12, 32);
    display.println("LORA TELEMETRY");
    display.setCursor(22, 46);
    display.println("BOOTING UP...");
    display.display();
  }

  // 2. Initialize Bluetooth Classic SPP
  SerialBT.begin("Drone_Telemetry");
  Serial.println("Bluetooth Started! Ready to pair as 'Drone_Telemetry'");

  // 3. Initialize LoRa SPI Bus
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa initialization failed!");
    if (oledReady) {
      display.clearDisplay();
      display.setCursor(0, 24);
      display.println("LORA INIT FAILED!");
      display.setCursor(0, 38);
      display.println("Check SPI wiring");
      display.display();
    }
    while (1);
  }

  // Matching RF settings with Air Unit
  LoRa.setSpreadingFactor(7);           // SF7 for fast throughput
  LoRa.setSignalBandwidth(250E3);        // 250 kHz bandwidth
  LoRa.setCodingRate4(5);               // 4/5 coding rate
  LoRa.setSyncWord(0x12);                // Private drone sync word
  LoRa.setTxPower(20);                   // +20dBm (100mW) output power

  updateStationBattery();
  delay(1000);
}

// =================== MAIN LOOP ===================
void loop() {
  // 1. Receive incoming LoRa packets from Drone -> Forward to Phone (BT) + Parse for OLED
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    while (LoRa.available()) {
      uint8_t b = LoRa.read();
      SerialBT.write(b);   // Send byte to QGroundControl on phone
      parseMavlinkByte(b); // Extract telemetry for local OLED display
    }
  }

  // 2. Receive commands from Phone (BT) -> Forward to Drone over LoRa
  if (SerialBT.available()) {
    uint8_t buffer[128];
    int count = 0;
    while (SerialBT.available() && count < sizeof(buffer)) {
      buffer[count++] = SerialBT.read();
    }
    if (count > 0) {
      LoRa.beginPacket();
      LoRa.write(buffer, count);
      LoRa.endPacket();
    }
  }

  // 3. Monitor Station 1S LiPo Battery every 2 seconds
  if (millis() - lastBatCheckTime > 2000) {
    lastBatCheckTime = millis();
    updateStationBattery();
  }

  // 4. Refresh OLED at 4 Hz (Smooth display without starving telemetry loop)
  if (millis() - lastDisplayUpdate > 250) {
    lastDisplayUpdate = millis();
    updateOLED();
  }
}
