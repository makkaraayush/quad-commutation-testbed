# Test 08: Outdoor RTL Flight & The Tree Rescue Incident

**Date:** October 6, 2026  
**Result:** Passed Autonomous RTL Twice (15m climb, pinpoint return to takeoff coordinates), followed by Low-Battery Failsafe Tree Snag and Barehanded Catch Recovery  
**Setup:** Outdoor autonomous navigation and Return-To-Launch testing in open airspace  

---

## Why I Did This Test
Following the successful indoor tests (Test 05 Arming, Test 06 Stabilization, and Test 07 Alt-Hold), it was time to take the 500mm quad outdoors to verify GPS-dependent autonomous navigation using the M10Q GPS and QMC5883L compass.

The primary objective was testing ArduPilot's **Return-To-Launch (RTL)** mode to confirm that:
1. The flight controller correctly records the Home takeoff coordinates upon arming.
2. When RTL is commanded via the FlySky transmitter switch bank, the quad autonomously climbs to its configured 15m return altitude (`RTL_ALT = 1500`), orients towards Home, and navigates back accurately.
3. The quad accurately positions itself above the launch point for an autonomous landing.

---

## Hardware Stack
* **Frame:** TBS 500 (Clone)
* **FC:** DakeFPV F405 (STM32F405)
* **Firmware:** ArduCopter Custom Firmware (V4.7.1)
* **ESCs:** 4x 30A Analog (SimonK/BLHeli, 480Hz PWM)
* **Motors:** 4x 2212 1000KV with 1045 props
* **Battery:** 3S 2200mAh LiPo
* **Radio:** FlySky FS-i6 TX / FS-iA6B RX (i-BUS, 5-mode mixer active)
* **Sensors:** M10Q 250 GPS + QMC5883L Compass

---

## What Happened: The RTL Flights & The Tree Snag

### 1. Flawless RTL Verification (2 Consecutive Passes)
I tested Return-To-Launch twice in the open field:
* In both test runs, engaging RTL on the transmitter worked perfectly.
* The quad smoothly climbed to exactly 15 meters altitude, aligned its heading towards the takeoff point, navigated across the field, and returned precisely to the exact takeoff location.
* GPS positioning and compass orientation were dead-on.

### 2. The Low-Battery Failsafe Trigger
After completing the tests, I began walking the drone back home. However, my battery failsafe was configured to execute RTL on low voltage (`BATT_FS_LOW_ACT = 2`).

Right at the exact moment the drone reached home, the 3S 2200mAh pack voltage dipped below the low-battery failsafe threshold. ArduPilot did exactly what it was programmed to do: it took autonomous control, climbed to the 15m RTL altitude, and flew in a direct straight line back toward the field takeoff coordinates.

Unfortunately, directly in the flight path between home and the takeoff site stood the tallest tree in the neighborhood (~18-20m tall). Cruising at 15m, the quad flew straight into the thick upper canopy and became firmly lodged in the branches.

---

## The Tree Rescue Mission Saga

Retrieving a 500mm quadcopter trapped 15 meters up in a tree turned into a full-scale rescue mission:

1. **Climbing the Tree:** Rushed over and climbed up the main trunk to attempt shaking the branches or knocking it free. Two friends stood on the ground stretching out a large cloth to catch the drone and absorb the fall. However, the upper branches where the drone was wedged were narrowing down rapidly and were far too thin and fragile to bear climbing weight without snapping.
2. **The Canopy Branch Poke:** While perched in the upper tree, I broke off a long dead branch from the tree itself and tried jabbing at the drone, but the quad was still just out of arm's reach.
3. **Emergency Call:** Called the local Fire Brigade for high-reach ladder assistance, but they clarified that their policy strictly limits deployments to human life-threatening emergencies.
4. **Engineering the Ultra-Long Rescue Pole:** Climbed down and engineered a custom high-reach pole: spliced an extremely long bamboo pole together with a lightweight PVC pipe, and lashed a wooden stiffening stick along the joint with tight rope bindings to keep it completely rigid under gravity.
5. **The Push:** Raised the extended pole up to the canopy. The two helpers pushed the drone free from the branches while I positioned myself below with the safety cloth.
6. **The Diving Catch:** As the drone fell, its trajectory veered sharply away from the cloth, heading straight toward the hard ground! On pure adrenaline, I dropped the cloth, sprinted across, and caught the falling quad mid-air barehanded!

---

## Post-Rescue Damage Assessment
* **Propellers:** Exactly 1 propeller snapped during the barehanded catch.
* **Frame & Carbon Arms:** Completely intact, zero cracks or bends.
* **Electronics & Sensors:** DakeFPV F405 FC, ESCs, 2212 motors, M10Q GPS, and QMC5883L compass all survived with zero damage and checked out 100% healthy.

---

## Engineering Post-Mortem & Failsafe Dilemma

This incident highlighted a classic autonomous failsafe trade-off:

* **The Cause:** `RTL_ALT` was set to the default 15m (1500cm). In suburban/neighborhood environments, mature tree crowns frequently exceed 15-20 meters.
* **Option A: Increase RTL Altitude to 20m - 25m**
  * *Pros:* Clears all neighborhood trees and power lines on any automated return path.
  * *Cons:* On my current 3S 2200mAh LiPo pack, climbing an extra 5-10 meters when the battery is already in low-voltage warning draws massive current. If the battery cuts out or browns out mid-air at 25 meters, the resulting freefall would completely obliterate the drone. This option must wait until I upgrade to a larger capacity battery (e.g., 4000mAh+).
* **Option B: Switch Low-Battery Failsafe to Land Immediately (`BATT_FS_LOW_ACT = 1`)**
  * *Pros:* Stops the drone from climbing on a dying battery and eliminates long-distance transit into obstacles.
  * *Cons:* Requires constant pilot vigilance to make sure the drone never flies over water, busy roads, or inaccessible rooftops where an immediate landing could cause loss.

---

## Next Steps
1. The 3S 2200mAh battery is currently charging on the bench.
2. Replace the broken propeller.
3. Reconnect to Mission Planner for complete post-incident sensor, calibration, and motor telemetry verification.
4. Adjust failsafe parameters prior to next outdoor session.
