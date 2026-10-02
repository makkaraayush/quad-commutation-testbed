# Test 01: Arming & Motor Check (Strapped Hardware Test)

**Date:** October 3, 2026  
**Result:** Passed (Hardware is solid, stock PIDs are way too aggressive)  
**Setup:** Strapped flat to the suitcase anchor testbed  

---

## Why I Did This Test
After replacing the fried ESC on Motor 4 and redoing the solder joints, I needed to make sure all four motors would actually start up and behave under real battery power with propellers attached. 

Because my last untethered test ended with shattered props against a wall, I made sure the drone was strapped down completely flat to the suitcase anchor platform before flipping the arm switch. No flying allowed here—just pure hardware verification to see if the ESCs sync up and the motors spin the right way without anything catching fire.

---

## The Whiteboard Breakdown
Here is everything I wrote on the whiteboard for this session:

* **Goal:** Arming & Motor Check
* **Description:** Hardware check while strapped
* **Hardware Stack:**
  * Frame: TBS 500 (Clone)
  * FC: DakeFPV F405 (flashed with DAKEFPVF405)
  * ESCs: 30A Analog (SimonK/BLHeli, +5V BEC wire disconnected)
  * Motors: 4x 2212 1000KV brushless
  * Battery: 3S 2200mAh LiPo
  * Radio: FlySky FS-i6 TX with FS-iA6B RX (i-BUS over UART2)
  * Propellers: 10-inch
  * GPS: None
* **Firmware:** Betaflight 4.4+
* **Tuning:** Stock

---

## The Tuning Preset I Used

Here is the exact PID profile I had loaded during this test:

![Stock Betaflight PID Preset](test_01_stock_pid_preset.png)

These are literally the default out-of-the-box Betaflight settings made for standard 5-inch freestyle quads:
* Roll: P: 45 | I: 80 | D: 30 | D Max: 40 | Feedforward: 120
* Pitch: P: 47 | I: 84 | D: 34 | D Max: 46 | Feedforward: 125
* Yaw: P: 45 | I: 80 | D: 0 | Feedforward: 120
* Master Multiplier: 1.00
* Feedforward Boost: 15, Max Rate Limit: 90
* Anti-Gravity: Enabled (Gain 8.0)

I left these stock on purpose just to see how the hardware would react before applying my own custom multipliers.

---

## What Happened in the Test Video

You can watch the full uncut test clip here: [Test 01 Video Recording](test_01_hardware_check_strapped.mp4)

Here is the play-by-play of how it went down:

1. **The Testbed:** The drone was strapped firmly onto the top of the suitcase with the weights sitting safely outside the prop arcs. It was locked down tight with zero room to lift or tilt.
2. **Arming:** Powered up the FlySky transmitter and flipped the arm switch (AUX 1). All four motors woke up instantly and spun up into idle together. No stutter, no desync, and no hesitation.
3. **Motor 4 Check:** The newly soldered ESC on Motor 4 worked like a charm. The weird electromagnetic brake and shorted MOSFET issue from before is completely gone. It spun freely and stayed cool.
4. **Throttle & Stick Response:** I slowly raised the throttle stick to check motor spool-up. The sound was clean and all four quadrants produced balanced thrust. Wiggling the pitch and roll sticks confirmed that the flight controller was sending differential commands properly.
5. **Why the motors were surging so hard:** In the video, you can clearly hear the motors revving up and hunting aggressively when throttle is applied. That is because these stock Betaflight PIDs are meant for tiny 600g 5-inch quads that snap around instantly. On a heavy 500mm frame swinging big 10-inch props, the inertia is huge. Because the drone was strapped down and could not actually move, the gyro saw zero tilt and the PID loop kept winding up harder and harder trying to force it to level. It proves beyond doubt that stock 5-inch tuning is way too aggressive for this frame.
6. **Disarm:** Flipped the disarm switch, and all four motors stopped instantly. Total test lasted about a minute, followed by a well-deserved thumbs up!

---

## What I Learned & What's Next
* **Hardware:** 100% good to go. The board, receiver link, failsafe, and all four ESCs/motors are working properly.
* **Tuning:** I definitely cannot fly it with stock 5-inch PIDs. I need to flash the custom low-gain profile from my CLI dump (cutting P and D in half, killing feedforward, and smoothing out the gyro filters).
* **Next Test:** Once the custom PIDs are loaded, I'll move to Test 02 on a tether that allows about 7 inches of vertical lift to see how it hovers.
