# Test 07: ArduPilot Altitude Hold (Alt-Hold) Test

**Date:** October 6, 2026  
**Result:** Passed (Rock-solid vertical altitude lock via onboard barometer and EKF, stable hands-off throttle, clean landing)  
**Setup:** Untethered indoor flight on floor  

---

## Why I Did This Test
With Test 06 proving manual attitude stabilization, the next step was verifying automated altitude control using ArduPilot's **Alt-Hold** mode.

In Alt-Hold, ArduPilot uses the onboard barometer fused with vertical accelerometer data via the Extended Kalman Filter (EKF) to automatically control motor throttle. Centering the throttle stick commands the flight controller to maintain current altitude, freeing the pilot from constantly managing vertical drift. Verifying this indoors tests how well the baro and EKF handle localized pressure fluctuations and ground effect turbulence before taking the quad outdoors for GPS-based flight modes.

---

## The Whiteboard Breakdown
Here is the whiteboard setup for this run:

* **Goal:** Achieve Successful Flight
* **Description:** Alt-Hold Test
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

You can watch the recording here: [Test 07 Video Recording](test_07_alt_hold_test.mp4)

Here is how the test went down:

1. **Liftoff & Engagement:** Took off smoothly in Stabilize, brought the quad up to roughly 1.5 meters, and toggled the 3-position mode switch to Alt-Hold (Bank 1, Position 2).
2. **Altitude Lock:** Centered the left throttle stick. The quad locked its vertical position in mid-air immediately.
3. **Stability in Ground Effect:** Despite operating indoors where 10-inch props recirculate turbulent air against walls and furniture, the barometer and EKF held altitude rock-solid with zero vertical hunting or oscillations.
4. **Positioning & Control:** Applied gentle cyclic corrections while altitude remained locked automatically.
5. **Descent & Landing:** Lowered the throttle stick out of the center deadband to initiate a controlled descent, touching down softly on the tile floor and disarming cleanly.

---

## Conclusion & Next Flight Milestones
Tests 05, 06, and 07 establish that the quad is thoroughly proven for baseline flight under ArduPilot:
* Arming and ESC synchronization verified.
* Manual attitude stabilization verified with zero wobble.
* Barometer-assisted Altitude Hold verified with pinpoint vertical hold.

With indoor envelope expansion complete, the next phase is taking the quad outdoors to an open flying field to test:
1. **Loiter Mode:** GPS position hold and wind rejection using the M10Q GPS and QMC5883L compass.
2. **Return-To-Launch (RTL):** Autonomous navigation back to the arming point and automated landing.
3. **In-Flight AutoTune:** Automated rate PID tuning in open airspace.
