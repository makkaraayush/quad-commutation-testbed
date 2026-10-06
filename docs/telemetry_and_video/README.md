# Complete Telemetry & Live Video Upgrade Guide

This guide documents the full build, wiring, flashing, and configuration of the custom long-range LoRa telemetry link, Bluetooth smartphone ground station, and short-range Wi-Fi video streaming system for our 500mm ArduPilot quadcopter.

---

## Why I Built My Own System (The Budget Reality)
Commercial drone telemetry systems (like SiK 433/915MHz telemetry radios, Holybro gear, or RFDesign modules) and dedicated 5.8GHz FPV video gear (video transmitters, receivers, and FPV monitors/goggles) are ridiculously expensive. As an independent builder on a strict budget, I simply could not afford spending hundreds of dollars on commercial ground station setups.

Instead of compromising or stalling the project, I decided to engineer my own system from scratch using accessible, low-cost microcontrollers:
* An **ESP32-CAM** ($4) handles dual-core onboard tasks: bridging telemetry and streaming live video.
* Two **Ra-02 433MHz LoRa modules** (~$2.50 each) provide 1-2 km of long-range, two-way MAVLink telemetry.
* A standard **ESP32 DevKit** (~$3) with a **0.96" OLED display** (~$1.50) serves as a standalone ground HUD mounted directly onto my FlySky FS-i6 radio.
* **Bluetooth Classic (SPP)** bridges telemetry directly into QGroundControl on my phone while keeping my phone's 4G/5G mobile internet completely active for live satellite maps.
* A **TP4056 Type-C board with a custom 3.3k ohm resistor modification** charges a 1S 3.7V 520mAh LiPo safely at ~360mA to power the ground station on the transmitter.

The entire system was built for under $15 total, works cleanly, and gets the job done without breaking the bank.


## 1. System Architecture Overview

```text
[DRONE (IN FLIGHT)]
  DakeFPV F405 FC (UART6)
         |
         | (MAVLink2 @ 19200 baud)
         v
  AI-Thinker ESP32-CAM (Dual-Core)
    |-- Core 0: MAVLink <-> LoRa SPI Bridge
    |     |
    |     v
    |   Ra-02 433 MHz LoRa Module (Air) ~~~~~~ 433 MHz RF (1-2 km) ~~~~~~+
    |                                                                    |
    |-- Core 1: Web Video Server                                         |
          |                                                              |
          v                                                              |
        2.4 GHz Wi-Fi AP ("DRONE_CAM")                                   |
          | (30-50m Range)                                               |
          v                                                              |
        [PHONE BROWSER]                                                  v
        http://192.168.4.1/stream                                [GROUND UNIT ON FS-i6]
        (Live MJPEG Video Feed)                                    Ra-02 433 MHz LoRa (Ground)
                                                                         |
                                                                         | (SPI)
                                                                         v
                                                                   ESP32 DevKit (Ground)
                                                                     |-- 0.96" I2C OLED (Live HUD)
                                                                     |-- 1S 520mAh LiPo + TP4056
                                                                     |-- Bluetooth Classic (SPP)
                                                                           |
                                                                           v
                                                                     [ANDROID PHONE]
                                                                     QGroundControl App
                                                                     (4G/5G Mobile Data ACTIVE)
```

### Why this design works so well:
1. **Long-Range Telemetry (1–2 km):** MAVLink telemetry (GPS location, artificial horizon, battery voltage, satellites, and flight modes) and two-way waypoint commands travel over 433 MHz LoRa.
2. **Bluetooth Ground Bridge:** The Ground ESP32 forwards MAVLink packets directly to your Android smartphone over Bluetooth Classic SPP (`Drone_Telemetry`). Because Bluetooth is used instead of Wi-Fi for telemetry, your phone's 4G/5G mobile internet stays completely active, allowing live Google/Mapbox satellite tile downloads, web browsing, and calls in the field.
3. **Dedicated Transmitter HUD:** The 0.96" OLED display mounted on your FlySky FS-i6 provides instant glanceable telemetry (flight mode, satellite count, drone battery %, transmitter station battery %, altitude, coordinates) even if your phone is in your pocket.
4. **Near-Range Video (30–50 m):** When the drone is near you during takeoff, landing, or low hovers, your phone can connect to the ESP32-CAM's Wi-Fi hotspot to view the live video feed in any web browser.

---

## 2. Bill of Materials (BOM)

### Drone Air Unit (On Drone)
* 1x AI-Thinker ESP32-CAM (with OV2640 camera module)
* 1x LoRa Ra-02 (SX1278 433 MHz) transceiver module
* 1x 433 MHz Antenna: Either a 16.4 cm length of solid copper wire (quarter-wave monopole) or an IPEX-to-SMA pigtail with a 433 MHz rubber duck antenna
* Power source: 5V BEC (rated at least 1.5A) from the DakeFPV F405 or PDB

### Ground Station (Mounted on FlySky FS-i6)
* 1x Standard ESP32 Development Board (ESP-WROOM-32, 30-pin or 38-pin DevKit v1)
* 1x LoRa Ra-02 (SX1278 433 MHz) transceiver module
* 1x 0.96" I2C OLED Display (SSD1306, JMD0.96D-1 / 128x64, address 0x3C)
* 1x 1S 3.7V 520mAh LiPo battery (dedicated station power)
* 1x TP4056 1A Li-Ion Charging Board with Type-C USB and current protection
* 1x 3.3k ohm (3k3) 1/4W resistor (for TP4056 charge current modification)
* 2x 100k ohm 1/4W resistors (for 1:1 voltage divider on ADC pin 34)
* 1x Small SPST toggle or slide power switch
* 1x 433 MHz Antenna (16.4 cm wire or rubber duck)

### Tools & Flashing Equipment
* 1x FTDI USB-to-TTL Serial Adapter (with 3.3V / 5V jumper)
* Hookup wire (28–30 AWG silicone wire for signal lines, 24–26 AWG for power lines)
* Soldering iron, solder, and heat shrink tubing
* Double-sided foam mounting tape or 3D printed brackets

---

## 3. Physical Placement & Hardware Mounting

### Drone Air Unit Placement (500mm Frame)
1. **ESP32-CAM Position:**
   - Mount on the top deck or forward nose plate of the 500mm TBS Discovery clone frame.
   - Angle the OV2640 camera lens slightly downward (~10 to 15 degrees) so that in forward pitch you see the horizon and ground rather than empty sky.
   - Use double-sided foam tape underneath the board to isolate the camera from motor vibrations.
2. **Ra-02 LoRa Module Position:**
   - Mount the Ra-02 module toward the rear or underbelly of the frame, well away from the high-current ESC battery leads and power distribution board to minimize electromagnetic interference.
3. **Air Antenna Orientation:**
   - Solder a 16.4 cm solid core wire (cut precisely to 164 mm for 433 MHz quarter-wave resonance) to the ANT pad of the Ra-02.
   - Route this antenna wire vertically downward through the bottom plate, letting it hang straight down inside a plastic straw or cable tie. Vertical orientation matches the ground station antenna polarization and keeps it away from carbon fiber arms.

### Ground Station Placement (FlySky FS-i6 Radio)
1. **ESP32 DevKit & Ra-02 Placement:**
   - Mount the ESP32 DevKit and Ra-02 on the rear upper housing or the rear handle area of the FlySky FS-i6 using heavy-duty foam tape or a lightweight enclosure.
2. **0.96" OLED Display (JMD0.96D-1):**
   - Mount on the top-front faceplate of the FS-i6 transmitter (above the main LCD screen or between the top switches).
   - Ensure the OLED does not obstruct the movement of the gimbals, antenna, or auxiliary mode switches (Switches A, B, C, D).
3. **1S 520mAh LiPo & TP4056 Charger:**
   - Mount the 520mAh LiPo flat against the rear casing.
   - Mount the TP4056 board with its Type-C charging port accessible on the edge for easy charging.
   - Install a small slide switch in series with the battery positive wire so the station can be turned completely off when not in use.
4. **Ground Antenna:**
   - Position the ground 433 MHz antenna vertically, matching the polarization of the drone's antenna.

---

## 4. Hardware Wiring & Schematics

### 1. Air Unit Wiring: F405 <-> ESP32-CAM <-> Ra-02

> [!WARNING]
> The SX1278 (Ra-02) operates strictly between 2.8V and 3.6V. Connecting 5V directly to the Ra-02 will permanently burn out the RF transceiver. Always power the Ra-02 from the regulated 3.3V (3V3) pin of the ESP32-CAM.

#### A. DakeFPV F405 to ESP32-CAM
| F405 Flight Controller Pad | ESP32-CAM Pin | Purpose |
| :--- | :--- | :--- |
| **5V** (BEC pad) | **5V** | Powers the ESP32-CAM, camera, and Ra-02 |
| **GND** | **GND** | Common ground reference |
| **T6** (UART6 TX) | **U0R** (GPIO 3) | F405 transmits MAVLink telemetry to ESP32 |
| **R6** (UART6 RX) | **U0T** (GPIO 1) | ESP32 transmits commands/waypoints to F405 |

*(Note: If UART6 is unavailable, you can also use UART4 with `T4` to `U0R` and `R4` to `U0T`, adjusting the ArduPilot serial index accordingly).*

#### B. ESP32-CAM to Air LoRa Ra-02 (SPI on SD Card Lines)
| Ra-02 Pin | ESP32-CAM Pin | Details |
| :--- | :--- | :--- |
| **3.3V** | **3V3** | Clean 3.3V regulated power from ESP32 |
| **GND** | **GND** | Ground reference |
| **NSS / CS** | **GPIO 15** | SPI Chip Select |
| **SCK** | **GPIO 14** | SPI Clock |
| **MOSI** | **GPIO 13** | SPI Master Out Slave In |
| **MISO** | **GPIO 2** | SPI Master In Slave Out |
| **RST** | **3V3** | Tie directly to 3.3V (software reset omitted to save pins) |
| **DIO0** | *Not Connected* | Handled via high-speed software polling |
| **ANT** | **16.4 cm Wire** | Soldered directly to center RF antenna pad |

---

### 2. Ground Station Wiring: ESP32 <-> Ra-02 <-> OLED <-> Battery

#### A. ESP32 DevKit to Ground LoRa Ra-02 (Hardware VSPI)
| Ra-02 Pin | ESP32 DevKit Pin | Purpose |
| :--- | :--- | :--- |
| **3.3V** | **3V3** | Regulated 3.3V power |
| **GND** | **GND** | Ground |
| **NSS / CS** | **GPIO 5** | SPI Chip Select |
| **SCK** | **GPIO 18** | SPI Clock |
| **MOSI** | **GPIO 23** | SPI MOSI |
| **MISO** | **GPIO 19** | SPI MISO |
| **RST** | **3V3** | Tied to 3.3V |
| **DIO0** | *Not Connected* | Polling mode |
| **ANT** | **16.4 cm Wire** | Soldered directly to center RF pad |

#### B. ESP32 DevKit to 0.96" OLED (JMD0.96D-1 SSD1306)
| OLED Pin | ESP32 DevKit Pin | Purpose |
| :--- | :--- | :--- |
| **VCC** | **3V3** | 3.3V Power |
| **GND** | **GND** | Ground |
| **SDA** | **GPIO 21** | I2C Data |
| **SCL** | **GPIO 22** | I2C Clock |

#### C. Battery Voltage Divider for ADC Pin 34
To safely measure the 1S LiPo voltage (3.4V to 4.2V) with the ESP32's 3.3V ADC:
```text
1S LiPo (+) -----> [100k Resistor] -----> Node (GPIO 34) -----> [100k Resistor] -----> GND
```
* With a 1:1 division factor, a fully charged 4.20V battery produces 2.10V at GPIO 34, which is well within the 0–3.3V ADC input range.

---

### 3. TP4056 Charger Modification (The 3.3k Resistor Swap)

> [!IMPORTANT]
> Standard TP4056 charging boards ship with a factory **1.2k ohm** SMD resistor at position **R3 / Rprog**, which sets the charging current to **1000 mA (1.0 Amp)**. Connecting a small **520mAh LiPo** to a 1000mA charger results in a nearly 2C charge rate, which can cause overheating and battery degradation.

#### The Math:
The TP4056 charge current formula is:
$$I_{\text{charge}} = \frac{1200\text{V}}{R_{\text{prog}}}$$

* Stock: $R_{\text{prog}} = 1.2\text{k}\Omega \rightarrow I_{\text{charge}} = 1000\text{ mA}$
* With 3.3k resistor: $R_{\text{prog}} = 3.3\text{k}\Omega \rightarrow I_{\text{charge}} \approx \mathbf{363\text{ mA}}$

A charging current of ~360 mA corresponds to **~0.7C** for your 520mAh cell, providing safe, gentle, and reliable charging without heating.

#### How to replace the resistor:
1. Locate the tiny surface-mount resistor labeled `122` (1.2k) or marked as `R3` near pin 2 of the TP4056 IC.
2. Touch the tip of your soldering iron to both sides to desolder and remove the stock resistor.
3. Solder your new **3.3k ohm (1/4W)** resistor across the two pads (bend the legs neatly and insulate with heat shrink if using a through-hole resistor).
4. Connect the battery:
   - 1S LiPo (+) -> `B+` on the TP4056 board
   - 1S LiPo (-) -> `B-` on the TP4056 board
   - `OUT+` -> Slide Switch -> ESP32 `VIN` (or 5V boost module)
   - `OUT-` -> ESP32 `GND`

---

## 5. Software & Arduino IDE Setup

1. **Add ESP32 Board Manager URL:**
   - In Arduino IDE, open **File -> Preferences**.
   - In "Additional Board Manager URLs", add:
     ```text
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
2. **Install ESP32 Core:**
   - Open **Tools -> Board -> Boards Manager**.
   - Search for `esp32` and install version **2.0.14** (recommended for camera and Bluetooth stability).
3. **Install Required Libraries:**
   - Open **Sketch -> Include Library -> Manage Libraries**.
   - Search for and install:
     - `LoRa` by Sandeep Mistry (version 0.8.0 or newer)
     - `Adafruit SSD1306` (version 2.5.7 or newer)
     - `Adafruit GFX Library`

---

## 6. Flashing Guide

### Step 1: Flashing the Air Unit (ESP32-CAM)
*Sketch location:* [`src/air_unit_esp32_cam/air_unit_esp32_cam.ino`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/air_unit_esp32_cam/air_unit_esp32_cam.ino)

1. Connect the FTDI adapter to the ESP32-CAM:
   - FTDI `5V` -> ESP32-CAM `5V`
   - FTDI `GND` -> ESP32-CAM `GND`
   - FTDI `TX` -> ESP32-CAM `U0R` (GPIO 3)
   - FTDI `RX` -> ESP32-CAM `U0T` (GPIO 1)
2. **Bridge `GPIO 0` to `GND`** with a jumper wire to enter bootloader mode.
3. In Arduino IDE, configure:
   - Board: **AI Thinker ESP32-CAM**
   - CPU Frequency: **240MHz (WiFi/BT)**
   - Flash Frequency: **80MHz**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - Upload Speed: **115200** or **460800**
4. Press the small reset button on the back of the ESP32-CAM, then click **Upload**.
5. Once "Done uploading" appears, **remove the jumper between `GPIO 0` and `GND`**, and press reset once more.

---

### Step 2: Bench Diagnostic Check (Ground Station)
*Sketch location:* [`src/ground_diagnostic/ground_diagnostic.ino`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/ground_diagnostic/ground_diagnostic.ino)

1. Plug the standard ESP32 DevKit into your PC via USB cable.
2. Select Board: **ESP32 Dev Module**, standard settings.
3. Upload the diagnostic sketch.
4. Open the Serial Monitor (115200 baud) and check the OLED display:
   - OLED should display `LoRa (433MHz): OK`
   - OLED should show live battery voltage (e.g., `3.85V (56%)`)
   - If LoRa reports FAIL, recheck SPI wiring on pins 18, 19, 23, and 5.

---

### Step 3: Flashing Ground Station Flight Firmware
*Sketch location:* [`src/ground_station_esp32/ground_station_esp32.ino`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/ground_station_esp32/ground_station_esp32.ino)

1. With the diagnostic passed, open and upload `ground_station_esp32.ino`.
2. The OLED will boot up showing `GROUND STATION LORA TELEMETRY BOOTING UP...`.
3. The board will broadcast as a Bluetooth device named **`Drone_Telemetry`**.

---

## 7. ArduPilot Parameter Configuration (Mission Planner)

Because LoRa operates with a smaller bandwidth window than a direct USB cable, we configure ArduPilot to stream essential flight telemetry (attitude, coordinates, battery, and mode) at clean, low packet rates:

1. Connect the drone's F405 to your PC via USB and open Mission Planner.
2. Navigate to **Config/Tuning -> Full Parameter List**.
3. Assuming you wired to UART6 (`T6`/`R6`), set the following parameters:
   - `SERIAL6_PROTOCOL` = **2** (MAVLink2)
   - `SERIAL6_BAUD` = **19** (19200 baud, matches the sketch)
   - `SR6_POSITION` = **2** (Streams GPS coordinates and altitude at 2 Hz)
   - `SR6_EXT_STAT` = **2** (Streams battery voltage and arm state at 2 Hz)
   - `SR6_EXTRA1` = **4** (Streams roll/pitch artificial horizon at 4 Hz)
   - `SR6_EXTRA2` = **2** (Streams airspeed and groundspeed HUD at 2 Hz)
   - `SR6_RAW_SENS` = **0** (Disables raw IMU vibration packets to save LoRa bandwidth)
   - `SR6_RC_CHAN` = **0** (Disables raw servo channel streams)
4. Click **Write Params**.
*(Note: If you wired to UART4 instead, set `SERIAL4_*` and `SR4_*` to these identical values).*

---

## 8. Android Phone & QGroundControl Setup

1. **Bluetooth Pairing:**
   - Power up the Ground Station on your FlySky transmitter.
   - On your Android phone, go to **Settings -> Bluetooth -> Pair new device**.
   - Tap **`Drone_Telemetry`** and confirm pairing.
2. **Connecting QGroundControl:**
   - Open the **QGroundControl** app on your phone.
   - Tap the **Q** logo in the top-left corner -> **Application Settings**.
   - Tap **Comm Links** -> **Add**:
     - Name: `LoRa Bluetooth`
     - Type: **Bluetooth**
     - Server / Device: Select **`Drone_Telemetry`** from the paired list
     - Check: **Automatically Connect on Start**
   - Tap **OK**, select the link, and tap **Connect**.
3. **Live Operation:**
   - Within 2–3 seconds, QGroundControl will announce "Vehicle Connected", the artificial horizon will respond, and satellite count and battery percentage will populate.
   - Your phone's **4G/5G mobile data stays active**, so satellite map tiles load automatically without needing local Wi-Fi.
4. **Offline Map Caching:**
   - In QGroundControl, go to **Application Settings -> Offline Maps**.
   - Tap **Add New Set**, drag the bounding box over your flying field, and download zoom levels 15–19 so maps work even in areas with zero cellular reception.

---

## 9. Short-Range Live Video Streaming

1. When the drone is powered on within 30–50 meters of you, open Wi-Fi settings on your phone or laptop.
2. Connect to the Wi-Fi network:
   - **SSID:** `DRONE_CAM`
   - **Password:** `12345678`
3. Open Google Chrome (or any web browser) and navigate to:
   ```text
   http://192.168.4.1/stream
   ```
4. You will see the continuous live MJPEG video stream from the OV2640 camera at ~20–25 FPS.

---

## 10. Autonomous Waypoint Mission Execution

A pre-configured safe test mission is provided in the repository at:
[`src/missions/park_test_mission.waypoints`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/missions/park_test_mission.waypoints)

### Safe Altitude Rule:
* As learned from our Test 08 tree incident, all navigation waypoints are set to **22.0 meters** to ensure the quad flies comfortably above the neighborhood tree crowns.

### How to upload and run the mission:
1. In QGroundControl, tap the **Plan** icon in the top toolbar.
2. Tap **File -> Open**, and load `park_test_mission.waypoints`.
3. Verify the route on your map.
4. Tap **Upload** (top right) to transmit the waypoints over Bluetooth -> Ground LoRa -> Air LoRa -> F405 EEPROM.
5. In the field, take off in Stabilize or Alt-Hold, verify stability, and flip your mode switch to **AUTO** to let ArduPilot execute the mission and return home.

---

## 11. Pre-Flight Bench Validation Checklist

Before flying with the new telemetry and video gear:

1. **Power Check:**
   - Power the Ground Station via its 1S 520mAh LiPo. Confirm the OLED boots up and shows a healthy voltage (e.g. `3.8V+`).
   - Power the drone via USB first (phone cable method) then LiPo. Confirm the ESP32-CAM red LED illuminates and does not brown out.
2. **Telemetry Link Check:**
   - Confirm the OLED switches from `NO TELEMETRY` to active flight mode (e.g. `STABILIZE` or `DISARMED`).
   - Confirm that lifting or tilting the drone by hand immediately updates the artificial horizon in QGroundControl.
3. **Range & Failsafe Check:**
   - Walk 30 meters away with the transmitter and confirm packet stream remains active.
   - Turn off the Ground Station while connected to QGroundControl: verify that QGroundControl announces "Telemetry Connection Lost", and verify that the drone continues to respond normally to your FlySky RC sticks.
4. **Antenna Polarization:**
   - Ensure the drone's 16.4 cm wire antenna hangs straight down vertically, and hold the transmitter antenna vertically for matched RF polarization.
