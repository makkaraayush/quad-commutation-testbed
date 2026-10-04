# Hardware Circuit & Wiring Guide

Here is the exact circuit breakdown and wiring connections for the 500mm quadcopter testbed (pre-GPS baseline), verified during Test 03 and Test 04.

---

## Flight Controller Pinout Reference

Top view of the DakeFPV F405 board and its edge solder pads:

![DakeFPV F405 Top Pads](../../assets/hardware/dakefpv_f405_top_pads.jpg)

### Top Edge Pad Reference (Left to Right):
```text
[ SBUS ] [ GND ] [ 5V ] [ R2 ] [ T2 ] [ GND ] [ BAT ] [ S1 ] [ S2 ] [ S3 ] [ S4 ] [ CUR ] [ R3 ]
```

---

## Complete Circuit Breakdown

### 1. Battery & Power Distribution (PDB)
* **Battery:** 3S 2200mAh LiPo (11.1V - 12.6V) plugged into the main XT60 connector.
* **ESC Power Distribution Board (Blue Plate):**
  * The main battery leads solder directly to the primary power pads on the central blue circular PDB plate.
  * The PDB splits full battery voltage out to all four ESCs via heavy-gauge DC power leads (Red = VBat, Black = Battery Ground).
* **Flight Controller Main Power:**
  * Spliced from the PDB's battery bus directly to the top edge of the F405:
    * **BAT** pad <- Positive battery lead (powers the FC onboard 5V/9V switching regulators and feeds the battery voltage sensor).
    * **GND** pad <- Negative battery lead (located right next to BAT on the top row).

---

### 2. ESC & Motor Connections
* **ESCs:** 4x 30A Analog ESCs (SimonK / BLHeli).
* **Motor Phase Wiring:**
  * Each ESC connects its three output motor phase wires directly to its 2212 1000KV brushless motor.
  * Motor spin directions are configured in Betaflight Quad-X layout:
    * Motor 1 (Rear Right): Counter-Clockwise (CCW)
    * Motor 2 (Front Right): Clockwise (CW)
    * Motor 3 (Rear Left): Clockwise (CW)
    * Motor 4 (Front Left): Counter-Clockwise (CCW)
* **PWM Signal Lines (Top Edge of F405):**
  * ESC 1 Signal -> **S1** pad
  * ESC 2 Signal -> **S2** pad
  * ESC 3 Signal -> **S3** pad
  * ESC 4 Signal -> **S4** pad
* **ESC Common Ground:**
  * The ground reference wires from all four ESC signal leads are spliced together into a single common ground line and soldered to the **GND** pad in that exact same top row on the F405.
* **+5V BEC Isolation:**
  * The +5V red wire on each ESC signal plug is disconnected/clipped and heat-shrunk. This keeps the ESC linear regulators from fighting the flight controller's onboard switching regulator, eliminating ground loop noise.

---

### 3. Receiver Circuit (FlySky FS-iA6B via i-BUS)
* **Protocol:** Serial i-BUS over 3 wires from the receiver's i-BUS/SERVO port to the top row of the F405:
  * **VCC (Power):** Solder to **5V** pad (top edge).
  * **GND:** Solder to **GND** pad (top edge).
  * **Signal:** Solder to **R2** pad (UART2 Serial RX). Configured in Betaflight as UART2 -> Serial RX with `IBUS` provider.

---

## Circuit Schematic Flow

```text
                  +---------------------------+
                  |  3S 2200mAh LiPo Battery  |
                  +-------------+-------------+
                                | (XT60)
                                v
               +---------------------------------+
               |  ESC Power Distribution Board   |
               |       (Blue PDB Plate)          |
               +---+--------+--------+-------+---+
                   |        |        |       |   |
          +--------+        |        |       |   +-----------------------+
          |                 |        |       |                           |
          v                 v        v       v                           v
     +---------+       +---------+  +---------+  +---------+         +-------+-------+
     | ESC 1   |       | ESC 2   |  | ESC 3   |  | ESC 4   |         | DakeFPV F405  |
     | 30A     |       | 30A     |  | 30A     |  | 30A     |         | Flight Control|
     +----+----+       +----+----+  +----+----+  +----+----+         +-------+-------+
          |                 |            |            |                      ^
          | 3-Phase         | 3-Phase    | 3-Phase    | 3-Phase              |
          v                 v            v            v                      |
     +---------+       +---------+  +---------+  +---------+                 |
     | Motor 1 |       | Motor 2 |  | Motor 3 |  | Motor 4 |                 |
     | 1000KV  |       | 1000KV  |  | 1000KV  |  | 1000KV  |                 |
     +---------+       +---------+  +---------+  +---------+                 |
          |                 |            |            |                      |
          | Signal (White)  |            |            |                      |
          +-----------------|------------|------------|---------> [S1]       |
                            | Signal     |            |                      |
                            +------------|------------|---------> [S2]       |
                                         | Signal     |                      |
                                         +------------|---------> [S3]       |
                                                      | Signal               |
                                                      +---------> [S4]       |
                                                                             |
     All 4 ESC Ground Wires Spliced Together -------------------> [GND]      |
     PDB Battery Main Power ------------------------------------> [BAT]      |
     PDB Battery Main Ground -----------------------------------> [GND]      |
                                                                             |
     FlySky FS-iA6B Receiver (i-BUS):                                        |
       - 5V Power ----------------------------------------------> [5V]       |
       - Ground ------------------------------------------------> [GND]      |
       - i-BUS Signal ------------------------------------------> [R2]-------+
```

---

## Planned Circuit Additions (Post Test 04)

1. **M10Q 250 GPS & 5883 Compass:**
   * **GPS:** Power (5V, GND) with TX to an available RX pad (e.g. `R1` or `R6`) and RX to TX pad (`T1` or `T6`) on the bottom row.
   * **Compass:** I2C bus wiring directly to the dedicated **SDA** and **SCL** pads on the bottom-right corner of the F405.
   * Mounted on a raised folding mast to keep the magnetometer clear of high-current magnetic fields from the PDB and battery leads.
2. **Flight Controller Vibration Isolation:**
   * The DakeFPV F405 will be remounted on a squarely cut credit card plate backed with double-sided foam tape to absorb high-frequency motor vibrations from the 500mm frame.
3. **Custom ArduPilot Build:**
   * Compiling a lightweight ArduPilot binary tailored to fit within the 1MB flash limit of the STM32F405 processor for autonomous waypoint flight.
