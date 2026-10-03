# Test 04: In-Flight Failsafe Verification

**Date:** October 4, 2026  
**Result:** Passed (Motors cut cleanly in mid-air, failsafe verified under real load)  
**Setup:** Untethered hover failsafe drop test  

---

## Why I Did This Test
Back on September 26, I tested the failsafe on the bench with props off, making sure the receiver told Betaflight to kill throttle when the remote was powered down. 

Now that Test 03 proved the quad can actually hover smoothly without wobbling, it was time to test the failsafe in a real flight scenario. The goal here was to make sure that if the transmitter ever drops signal, runs out of battery, or disconnects in the air, the quad will instantly shut down its motors rather than shooting up into the ceiling or flying away.

---

## The Whiteboard Breakdown
Here is the whiteboard setup for this safety verification run:

* **Goal:** Achieve Stable Flight
* **Description:** Failsafe Check
* **Hardware Stack:**
  * Frame: TBS 500 (Clone)
  * FC: DakeFPV F405
  * ESCs: 30A Analog (SimonK/BLHeli)
  * Motors: 4x 2212 1000KV (1045 props)
  * Battery: 3S 2200mAh LiPo
  * Radio: FlySky FS-i6 TX / FS-iA6B RX (i-BUS)
  * GPS: None
* **Firmware:** Betaflight
* **Tuning:** Custom (same stable tune from Test 03)

---

## What Happened in the Test Video

You can watch the recording here: [Test 04 Video Recording](test_04_failsafe_check.mp4)

Here is how the test went down:

1. **Liftoff:** Armed and smoothly brought the quad up into a stable hover roughly eye level in the middle of the room.
2. **Stable Hold:** The drone held its attitude steadily, showing that the Test 03 tuning refinements are consistent and repeatable.
3. **Triggering Failsafe:** With the drone hovering steadily, I triggered the signal loss condition.
4. **The Drop:** Exactly as configured, all four motors instantly cut power at around the 0:36 mark, and the quad dropped straight down flat onto the floor.
5. **Aftermath:** The landing gear absorbed the impact, props stayed intact, and the motors remained completely dead. No delayed throttle spool-up, no flyaway risk.

---

## Conclusion & Next Hardware Steps
This officially completes the baseline safety and stabilization testing phase on Betaflight. We have:
* Confirmed motor commutation, sync, and ESC repair under load.
* Solved the 500mm frame wobble through low-gain PID and filter tuning.
* Verified that the emergency failsafe cuts motor power immediately in flight.

Now I am ready to move to hardware expansions:
1. Mount the folding GPS mast and wire up the M10Q 250 GPS and 5883 compass.
2. Remount the F405 flight controller on a credit card base with double-sided foam tape to dampen frame vibrations.
3. Compile a custom lightweight ArduPilot build tailored to the 1MB flash limit of this F405 board.
