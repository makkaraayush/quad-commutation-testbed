/*
  Ground Station Hardware Diagnostic & Bench Test
  Tests the OLED (JMD0.96D-1 SSD1306), Ra-02 LoRa module, and 1S LiPo battery ADC.
  Upload this first to verify all solder joints and pinouts before flying.
*/

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Hardware Pinout for Standard ESP32 DevKit
#define LORA_SCK        18
#define LORA_MISO       19
#define LORA_MOSI       23
#define LORA_SS         5
#define LORA_RST        -1
#define LORA_DIO0       -1

#define OLED_SDA        21
#define OLED_SCL        22
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDR_1   0x3C
#define SCREEN_ADDR_2   0x3D

#define STATION_BAT_PIN 34

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool loraOk = false;
bool oledOk = false;
uint8_t activeOledAddr = 0x3C;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n======================================");
  Serial.println("  GROUND STATION HARDWARE DIAGNOSTIC  ");
  Serial.println("======================================");

  // 1. Initialize I2C and check OLED address
  Wire.begin(OLED_SDA, OLED_SCL);
  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR_1)) {
    oledOk = true;
    activeOledAddr = SCREEN_ADDR_1;
  } else if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR_2)) {
    oledOk = true;
    activeOledAddr = SCREEN_ADDR_2;
  }

  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("HARDWARE DIAGNOSTIC");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(0, 18);
    display.println("Scanning hardware...");
    display.display();
  }

  // 2. Initialize SPI and LoRa module
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  loraOk = LoRa.begin(433E6);

  // 3. Print test results to USB Serial
  Serial.print("OLED (SSD1306 @ 0x");
  Serial.print(activeOledAddr, HEX);
  Serial.print("): ");
  Serial.println(oledOk ? "PASSED" : "FAILED (Check SDA:21, SCL:22, VCC, GND)");

  Serial.print("LoRa (Ra-02 433MHz): ");
  Serial.println(loraOk ? "PASSED" : "FAILED (Check SPI wiring and 3.3V rail)");

  // Configure ADC for 1S LiPo monitoring
  analogSetPinAttenuation(STATION_BAT_PIN, ADC_11db);
}

void loop() {
  // Read Station Battery Voltage (100k/100k divider on Pin 34)
  uint32_t rawSum = 0;
  for (int i = 0; i < 16; i++) {
    rawSum += analogRead(STATION_BAT_PIN);
    delay(2);
  }
  float rawAvg = rawSum / 16.0;
  
  // 3.3V ref / 4095 * 2.0 (divider factor) * 1.05 (ADC non-linearity compensation)
  float batVolts = (rawAvg / 4095.0) * 3.3 * 2.0 * 1.05;
  int batPct = constrain((int)((batVolts - 3.40) / (4.20 - 3.40) * 100.0), 0, 100);

  // Update OLED if operational
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Header
    display.setCursor(0, 0);
    display.println("HARDWARE DIAGNOSTIC");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // LoRa Status
    display.setCursor(0, 16);
    display.print("LoRa (433MHz): ");
    display.println(loraOk ? "OK" : "FAIL");

    // ADC Raw & Voltage
    display.setCursor(0, 28);
    display.print("ADC Raw (P34): ");
    display.println((int)rawAvg);

    display.setCursor(0, 40);
    display.print("1S LiPo Volts: ");
    display.print(batVolts, 2);
    display.print("V (");
    display.print(batPct);
    display.println("%)");

    // Footer
    display.setCursor(0, 54);
    if (loraOk) {
      display.println("READY FOR FLIGHT CODE");
    } else {
      display.println("CHECK LORA WIRING!");
    }

    display.display();
  }

  // Debug output over Serial
  Serial.print("LoRa: ");
  Serial.print(loraOk ? "OK" : "FAIL");
  Serial.print(" | ADC Raw: ");
  Serial.print((int)rawAvg);
  Serial.print(" | Bat: ");
  Serial.print(batVolts, 2);
  Serial.print("V (");
  Serial.print(batPct);
  Serial.println("%)");

  delay(600);
}
