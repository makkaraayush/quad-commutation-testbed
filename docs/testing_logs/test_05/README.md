# Test 05: ArduPilot Arming & Idle Motor Check

**Date:** October 6, 2026  
**Result:** Passed (Clean arming, instant idle spin across all 4 motors, immediate disarm)  
**Setup:** Untethered ground arming test on floor  

---

## Why I Did This Test
After switching from Betaflight to ArduPilot, fixing the compass I2C bus conflict (`NTF_LED_TYPES = 1`), establishing the field startup sequence for the analog ESCs (USB-C phone power trick), and remapping the motors to ArduCopter Quad-X standards, I needed to verify arming and low-throttle synchronization before attempting any flight.

The goal was to confirm:
1. ArduPilot passes all pre-arm checks with the M10Q GPS and QMC5883L compass active.
2. The arm switch on the FlySky FS-i6 triggers clean arming with no delays.
3. All four 2212 motors spin up smoothly at idle speed with zero stutter or desync.
4. The disarm switch instantly cuts motor power.

---

## The Whiteboard Breakdown
Here is the whiteboard setup for this run:

* **Goal:** Achieve Successful Flight
* **Description:** Arm Test
* **Hardware Stack:**
  * Frame: TBS 500 Clone
  * FC: DakeFPV F405
  * ESCs: 30A Analog (SimonK/BLHeli, 480Hz PWM)
  * Motors: 4x 2212 1000KV (1045 props)
  * Battery: 3S 2200mAh LiPo
  * Radio: FlySky FS-i6 TX / FS-iA6B RX (i-BUS)
  * GPS: M10Q
* **Firmware:** ArduPilot Custom Firmware
* **Tuning:** Custom

---

## What Happened in the Test Video

You can watch the recording here: [Test 05 Video Recording](test_05_arm_test.mp4)

Here is how the test went down:

1. **Pre-Arm Readiness:** Quad powered up cleanly via the phone boot sequence. ArduPilot completed its sensor and EKF checks.
2. **Arming Trigger:** Flipped the arm switch on the FlySky transmitter.
3. **Motor Spin-Up:** All four 1045 props spun up cleanly at idle without any stator chatter, stutter, or hesitation.
4. **Throttle Blip:** Tested small throttle blips from idle to verify that all four ESCs respond synchronously to throttle inputs without drawing the quad off balance.
5. **Clean Disarm:** Toggled the arm switch back to disarm; all four motors stopped immediately.

---

## Conclusion
Arming behavior, radio link, and ESC startup synchronization under ArduPilot are verified and solid. Ready to proceed to liftoff and manual stabilization testing.
