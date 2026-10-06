# How I Built My Custom LoRa Telemetry, Ground HUD & Live Video System

Commercial drone telemetry radios (like SiK 433/915MHz sets or RFD900 links) and dedicated 5.8GHz FPV video gear (VTX, receivers, FPV goggles/screens) are way too expensive. I'm building this 500mm quad on a student budget, and dropping $150 to $300 on commercial ground station equipment just wasn't an option.

Instead of giving up on telemetry or waiting until I can save up money, I decided to build my own system from scratch for under $15 using off-the-shelf microcontrollers. 

Here is the complete walkthrough of how I designed it, how I'm wiring everything up, and how you can replicate it step-by-step.

---

## 1. System Overview: How It All Works

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
    |   (16.4cm Antenna facing UP)                                       |
    |                                                                    |
    |-- Core 1: Live Video Web Server                                    |
          |                                                              |
          v                                                              |
        2.4 GHz Wi-Fi AP ("DRONE_CAM")                                   |
          | (30-50m Range)                                               |
          v                                                              |
        [PHONE BROWSER]                                                  v
        http://192.168.4.1/stream                                [GROUND UNIT ON FS-i6]
        (Live MJPEG Video Feed)                                    Ra-02 433 MHz LoRa (Ground)
                                                                   (16.4cm Antenna facing UP)
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

### Why this setup is so practical:
* **Long-Range Telemetry (1–2 km):** Telemetry packets (attitude, GPS coordinates, battery voltage, flight mode, satellites) travel over punchy 433 MHz LoRa between two cheap Ra-02 modules.
* **Bluetooth to Phone (Keeps Mobile Data Active!):** Instead of using Wi-Fi for telemetry (which disables your phone's mobile data), the Ground ESP32 forwards MAVLink over Bluetooth Classic SPP (`Drone_Telemetry`). Your phone's 4G/5G data stays completely active, meaning live Google/Mapbox satellite maps load smoothly in QGroundControl.
* **Glanceable Transmitter HUD:** The 0.96" OLED screen sits right on the top face of my FlySky FS-i6, so I can see my flight mode, battery percentage, altitude, and satellite lock with a quick glance without pulling my phone out.
* **Close-Range Live Video (30–50 m):** When the quad is close by during takeoff, hover, or landing, I can connect to the ESP32-CAM's Wi-Fi hotspot on any browser and see live video from the OV2640 camera.

---

## 2. Parts List & Hardware

### Drone Air Unit (On Drone)
* 1x AI-Thinker ESP32-CAM (with OV2640 camera module)
* 1x LoRa Ra-02 (SX1278 433 MHz) transceiver module
* 1x 16.4 cm solid copper wire (quarter-wave antenna cut to 164 mm)
* 5V power from the DakeFPV F405 flight controller BEC

### Ground Station (Mounted on FlySky FS-i6 Radio)
* 1x Standard ESP32 Development Board (ESP-WROOM-32 / 30-pin or 38-pin DevKit v1)
* 1x LoRa Ra-02 (SX1278 433 MHz) transceiver module
* 1x 0.96" I2C OLED Display (SSD1306, JMD0.96D-1 / 128x64, address 0x3C)
* 1x 1S 3.7V 520mAh LiPo battery
* 1x TP4056 Type-C Li-Ion battery charging board with protection
* 1x 3.3k ohm (3k3) 1/4W resistor (for modifying the TP4056 charge current)
* 2x 100k ohm 1/4W resistors (for the battery voltage divider on ADC pin 34)
* 1x Small SPST slide switch (Power ON/OFF)
* 1x 16.4 cm solid copper wire (quarter-wave antenna cut to 164 mm)

---

## 3. Physical Placement & Hardware Mounting

### On the 500mm Quad:
* **ESP32-CAM:** Mounted on the front nose / upper deck of the frame using double-sided foam tape to absorb motor vibrations. The camera lens is angled slightly downward (~10 to 15 degrees) so the horizon is centered during forward pitch.
* **Ra-02 Module:** Mounted on the rear top deck, well away from the high-current battery leads and PDB to keep RF noise low.
* **Air Antenna:** Solder a 16.4 cm wire directly to the **ANT pad on the Ra-02 module**. Route this wire **facing straight UP** (supported inside a plastic straw or attached to a small vertical zip tie). Keeping it pointing UP prevents it from dragging on the ground during landings or getting caught in obstacles.

### On the FlySky FS-i6 Transmitter:
* **ESP32 DevKit & Ra-02:** Mounted neatly on the rear casing or upper handle area of the radio using double-sided foam tape or a lightweight project box.
* **0.96" OLED Display (JMD0.96D-1):** Mounted on the top-front faceplate of the FS-i6 (just above the factory LCD screen or between the top toggle switches) where it is directly in your line of sight without interfering with the stick gimbals.
* **1S 520mAh LiPo & TP4056 Charger:** Mounted on the rear casing. The Type-C charging port is positioned facing outward on the side or bottom edge so you can easily plug in a phone charger cable.
* **Ground Station Power Switch (Where to put it):**
  - **Recommended Spot:** Place the small slide switch on the **top-right rear shoulder** (near the base of the handle / top-right corner of the rear module casing).
  - *Why this spot is ideal:*
    1. **Zero accidental bump risk:** Your palms wrap around the side grips of the FS-i6, while your fingers rest on the front switches. Having the slide switch on the top-right rear shoulder keeps it completely safe from accidental nudges while flying.
    2. **Quick index-finger access:** You can easily flick it ON or OFF with your right index finger without having to flip the transmitter around.
    3. **Short, tidy wiring:** It sits just an inch away from both the TP4056 board and the ESP32 `VIN` pin, keeping your power leads neat and short.
* **Ground Antenna:** Solder a 16.4 cm wire to the **ANT pad of the ground Ra-02**, mounted vertically pointing straight UP to match the drone's antenna polarization.


---

## 4. Ground Station Power, Battery & Charging Setup

This is the most critical part of the ground station build. Here is how I set up the battery, charging, and power switch so it charges cleanly from a standard phone adapter.

### A. The TP4056 3.3k Resistor Modification (CRITICAL FOR 520mAh LIPO)

> [!WARNING]
> Stock TP4056 boards ship from the factory with a **1.2k ohm** SMD resistor at position **R3 / Rprog**, which sets the charging current to **1000 mA (1.0 Amp)**. 
> 
> My ground station battery is only **520mAh**! Charging a 520mAh cell at 1000mA is nearly a 2C charge rate, which will overheat the battery, cause cell puffing, and ruin it.

The TP4056 charge current formula is:
$$I_{\text{charge}} = \frac{1200\text{V}}{R_{\text{prog}}}$$

* Stock resistor: $R_{\text{prog}} = 1.2\text{k}\Omega \rightarrow I_{\text{charge}} = 1000\text{ mA}$ (Too high!)
* With our 3.3k resistor: $R_{\text{prog}} = 3.3\text{k}\Omega \rightarrow I_{\text{charge}} \approx \mathbf{363\text{ mA}}$

Charging a 520mAh LiPo at ~360mA corresponds to **~0.7C**, which is the gentle, safe, industry-standard charging rate for lithium polymer cells.

#### How to swap the resistor:
1. Look near pin 2 of the TP4056 chip on the board for the small SMD resistor labeled `122` (or marked `R3` / `Rprog`).
2. Touch your soldering iron to both sides to desolder and remove it.
3. Solder your new **3.3k ohm (3k3) 1/4W resistor** across those two pads. If using a through-hole resistor, trim the legs short and solder them flat against the pads.

---

### B. Wiring the Battery, TP4056, Power Switch & ESP32

Here is the exact circuit diagram for the power section:

```text
       [Type-C Phone Charger]
                 |
                 v
        +-----------------+
        |  TP4056 Board   |
        |  (with 3.3k mod)|
        +-----------------+
          | B+        B- |
          |              |
          +-------+      +-------+
                  |              |
                ( + )          ( - )
             [1S 3.7V 520mAh LiPo Battery]
                  |              |
          +-------+      +-------+
          |              |
          | OUT+    OUT- |
          +---+      +---+
              |          |
              v          |
        [Slide Switch]   |
              |          |
              +----+     |
                   |     |
                   v     v
              [ESP32 DevKit]
               VIN       GND
```

#### Step-by-Step Power Connections:
1. **Battery to TP4056:**
   - Red wire of 1S LiPo (+) -> Solder to **`B+`** pad on TP4056.
   - Black wire of 1S LiPo (-) -> Solder to **`B-`** pad on TP4056.
2. **Ground to ESP32:**
   - Solder a wire from **`OUT-`** on TP4056 directly to a **`GND`** pin on the ESP32 DevKit.
3. **Power Switch to ESP32:**
   - Solder a wire from **`OUT+`** on TP4056 to the center pin of your SPST slide switch.
   - Solder a wire from the other pin of the slide switch to the **`VIN`** (or `5V`) pin on the ESP32 DevKit.
4. **How Charging Works:**
   - Whenever the battery gets low, simply plug any standard Type-C mobile charger cable into the TP4056 port.
   - The **Red LED** on the board lights up while charging.
   - The **Blue / Green LED** lights up when charging is complete (4.20V cut-off).
   - Because the slide switch sits between `OUT+` and the ESP32, you can flip the switch **OFF** while charging so the battery charges cleanly without powering the microcontroller.

---

### C. Measuring Station Battery Voltage on the OLED (100k / 100k Divider)

The ESP32's internal analog-to-digital converter (ADC) can only safely read up to 3.3V, but a freshly charged 1S LiPo reaches 4.20V. To safely display the battery percentage on our OLED screen:

```text
From Slide Switch (Switched Bat +) 
          |
          v
   [100k Resistor]
          |
          +-----> Connected directly to ESP32 Pin GPIO 34
          |
   [100k Resistor]
          |
          v
      ESP32 GND
```

* This 1:1 voltage divider cuts the voltage exactly in half (4.20V becomes 2.10V), which GPIO 34 reads safely without any risk of damaging the ESP32.

---

## 5. Ground Station Pinout: ESP32 to OLED & Ra-02

Here is how all the modules on the FlySky transmitter connect to the ESP32 DevKit:

### A. OLED Display (JMD0.96D-1 SSD1306, 4-Pin I2C)
| OLED Pin | ESP32 DevKit Pin | Purpose |
| :--- | :--- | :--- |
| **VCC** | **3V3** | 3.3V power |
| **GND** | **GND** | Ground |
| **SDA** | **GPIO 21** | I2C Data line |
| **SCL** | **GPIO 22** | I2C Clock line |

### B. Ground LoRa Module (Ra-02 433MHz over VSPI)

> [!WARNING]
> Connect Ra-02 power strictly to the **3.3V (3V3)** pin on the ESP32! Connecting 5V directly to the Ra-02 will destroy the SX1278 transceiver.

| Ra-02 Pin | ESP32 DevKit Pin | Details |
| :--- | :--- | :--- |
| **3.3V** | **3V3** | Clean regulated 3.3V power |
| **GND** | **GND** | Ground reference |
| **NSS / CS** | **GPIO 5** | SPI Chip Select |
| **SCK** | **GPIO 18** | SPI Clock |
| **MOSI** | **GPIO 23** | SPI Master Out Slave In |
| **MISO** | **GPIO 19** | SPI Master In Slave Out |
| **RST** | **3V3** | Tie directly to 3.3V (saves a GPIO pin) |
| **DIO0** | *Not Connected* | Polling mode in firmware |
| **ANT** | **16.4 cm Wire** | **Soldered directly to the ANT pad on the Ra-02 board** (pointing UP!) |

*(Note: The antenna connects strictly to the Ra-02's center ANT solder pad. Do not connect it to any pin on the ESP32!)*

---

## 6. Drone Air Unit Pinout: F405 to ESP32-CAM to Ra-02

On the drone, the AI-Thinker ESP32-CAM uses its dual-core processor to handle MAVLink bridging on Core 0 and live Wi-Fi video streaming on Core 1.

### A. DakeFPV F405 to ESP32-CAM
| F405 FC Pad | ESP32-CAM Pin | Purpose |
| :--- | :--- | :--- |
| **5V** (BEC pad) | **5V** | Powers the ESP32-CAM, camera, and Ra-02 |
| **GND** | **GND** | Common ground reference |
| **T6** (UART6 TX) | **U0R** (GPIO 3) | F405 sends MAVLink telemetry to ESP32 |
| **R6** (UART6 RX) | **U0T** (GPIO 1) | ESP32 sends waypoint commands to F405 |

### B. ESP32-CAM to Air LoRa Ra-02 (SPI on SD Card Lines)
Because the OV2640 camera occupies most pins on the ESP32-CAM, the Ra-02 connects over the SD card bus lines:

| Ra-02 Pin | ESP32-CAM Pin | Details |
| :--- | :--- | :--- |
| **3.3V** | **3V3** | Regulated 3.3V from ESP32-CAM |
| **GND** | **GND** | Ground |
| **NSS / CS** | **GPIO 15** | SPI Chip Select |
| **SCK** | **GPIO 14** | SPI Clock |
| **MOSI** | **GPIO 13** | SPI MOSI |
| **MISO** | **GPIO 2** | SPI MISO |
| **RST** | **3V3** | Tied to 3.3V |
| **DIO0** | *Not Connected* | Polling mode |
| **ANT** | **16.4 cm Wire** | **Soldered directly to ANT pad on Ra-02** (pointing straight UP!) |

---

## 7. Arduino IDE Setup & Flashing Instructions

### Required Libraries
In Arduino IDE, open **Sketch -> Include Library -> Manage Libraries**, and install:
* `LoRa` by Sandeep Mistry (version 0.8.0+)
* `Adafruit SSD1306`
* `Adafruit GFX Library`

---

### Step 1: Bench Testing the Ground Station
Before closing up the case on your FlySky transmitter, upload our diagnostic sketch:
*Sketch location:* [`src/ground_diagnostic/ground_diagnostic.ino`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/ground_diagnostic/ground_diagnostic.ino)

1. Connect the ESP32 DevKit to your PC via Micro-USB / Type-C.
2. Select Board: **ESP32 Dev Module**.
3. Upload the sketch.
4. The OLED display should show:
   - `LoRa (433MHz): OK`
   - `1S LiPo Volts: 3.85V (56%)`
   - If LoRa reports FAIL, recheck your SPI wiring on pins 18, 19, 23, and 5.

---

### Step 2: Uploading Ground Station Flight Code
*Sketch location:* [`src/ground_station_esp32/ground_station_esp32.ino`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/ground_station_esp32/ground_station_esp32.ino)

1. Open `ground_station_esp32.ino`.
2. Upload to the ESP32 DevKit.
3. The OLED will boot up with `GROUND STATION LORA TELEMETRY BOOTING UP...`.
4. It will broadcast as a Bluetooth device named **`Drone_Telemetry`**.

---

### Step 3: Flashing the Air Unit (ESP32-CAM)
*Sketch location:* [`src/air_unit_esp32_cam/air_unit_esp32_cam.ino`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/air_unit_esp32_cam/air_unit_esp32_cam.ino)

1. Connect your FTDI adapter to the ESP32-CAM:
   - FTDI `5V` -> ESP32-CAM `5V`
   - FTDI `GND` -> ESP32-CAM `GND`
   - FTDI `TX` -> ESP32-CAM `U0R` (GPIO 3)
   - FTDI `RX` -> ESP32-CAM `U0T` (GPIO 1)
2. **Connect a jumper wire between `GPIO 0` and `GND`** (puts the ESP32-CAM in flash mode).
3. In Arduino IDE:
   - Board: **AI Thinker ESP32-CAM**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - Upload Speed: **115200**
4. Press the small reset button on the ESP32-CAM and hit **Upload**.
5. Once flashing finishes, **disconnect `GPIO 0` from `GND`** and tap reset.

---

## 8. ArduPilot Configuration (Mission Planner)

Because LoRa has a narrower bandwidth window than a direct USB cable, we configure ArduPilot to stream essential telemetry (attitude, GPS, battery, flight mode) without flooding the radio link:

1. Connect the F405 flight controller to Mission Planner via USB.
2. Go to **Config/Tuning -> Full Parameter List**.
3. Set the following parameters (assuming UART6 on `T6`/`R6`):
   - `SERIAL6_PROTOCOL` = **2** (MAVLink2)
   - `SERIAL6_BAUD` = **19** (19200 baud)
   - `SR6_POSITION` = **2** (GPS position & altitude at 2 Hz)
   - `SR6_EXT_STAT` = **2** (Battery voltage and arm state at 2 Hz)
   - `SR6_EXTRA1` = **4** (Attitude / artificial horizon at 4 Hz)
   - `SR6_EXTRA2` = **2** (Speed and HUD at 2 Hz)
   - `SR6_RAW_SENS` = **0** (Disables raw IMU vibration data to save bandwidth)
   - `SR6_RC_CHAN` = **0** (Disables raw servo channel streams)
4. Click **Write Params**.

---

## 9. Operating in the Field

### Connecting Telemetry to Your Phone (QGroundControl):
1. Turn on the Ground Station switch on your FlySky transmitter.
2. On your Android phone, go to **Settings -> Bluetooth** and pair with **`Drone_Telemetry`**.
3. Open **QGroundControl**:
   - Go to **Application Settings -> Comm Links -> Add**.
   - Type: **Bluetooth** -> Select **`Drone_Telemetry`**.
   - Check **Automatically Connect on Start** and click **Connect**.
4. The map will immediately snap to your drone's GPS position, the artificial horizon will level out, and voice alerts will call out battery voltage and satellite lock.
5. **Your phone's 4G/5G mobile internet stays active the entire time**, so satellite map tiles load smoothly as you fly.

### Viewing Close-Range Video:
1. In your phone's Wi-Fi settings, connect to **`DRONE_CAM`** (Password: `12345678`).
2. Open Google Chrome or any browser and go to:
   ```text
   http://192.168.4.1/stream
   ```
3. You get a live video feed directly from the drone at ~25 FPS.

### Running Safe Autonomous Missions:
A ready-to-fly waypoint mission is available in the repository at:
[`src/missions/park_test_mission.waypoints`](file:///d:/MIT/Projects/Drone/quad-commutation-testbed/src/missions/park_test_mission.waypoints)

* All waypoints are set to **22.0 meters** altitude so the quad easily clears any park trees on autonomous flights.
