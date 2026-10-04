# Test 05: ArduPilot Maiden Flight & Aggressive Maneuver Test

**Date:** October 5, 2026  
**Result:** Crashed during hard maneuver, but safely recovered; overall flight confirmed very stable  
**Setup:** Untethered outdoor free flight (off-camera)  

---

## Why I Did This Test
With ArduCopter V4.7.1 successfully flashed, the QMC5883L compass running clean on I2C, the analog ESC boot sequence worked out with the phone cable trick, and the FlySky FS-i6 configured for 5 flight modes, it was time for the quad's first real maiden flights under ArduPilot.

The goal was to evaluate baseline flight stability, test response in Stabilize and AltHold, and push into more aggressive maneuvers to see how the 500mm frame and 10-inch props handle dynamic inputs.

---

## Hardware Stack
* **Frame:** TBS 500 (Clone)
* **FC:** DakeFPV F405 (STM32F405)
* **Firmware:** ArduCopter V4.7.1
* **ESCs:** 4x 30A Analog (SimonK/BLHeli, 480Hz PWM)
* **Motors:** 4x 2212 1000KV with 1045 props
* **Battery:** 3S 2200mAh LiPo
* **Radio:** FlySky FS-i6 TX / FS-iA6B RX (i-BUS, 5-mode mixer active)
* **Sensors:** M10Q 250 GPS + QMC5883L Compass

---

## What Happened During the Flight
* **Off-Camera Testing:** I didn't have access to a camera while running this test session, so this flight was done completely off-camera.
* **Flight Stability:** In normal flight and hover, the quad is actually very stable. The ArduPilot attitude estimation and filtering handle the 500mm frame significantly better than early Betaflight default gains.
* **The Crash:** During one of the aggressive testing runs, pushing hard into dynamic maneuvers to see how the quad responds under aggressive attitude changes caused the drone to crash.
* **Damage Assessment:** Thankfully, the quad was recovered safely without catastrophic damage. The core frame, motors, and electronics are all safe and operational.

---

## Takeaways & Next Steps
1. The drone has proven its baseline stability under ArduPilot.
2. Routine pre-flight safety check: inspect motor mounts, prop integrity, and arm rigidity following the impact.
3. Bring a camera for the next session and prepare for a clean AltHold hover test to engage AutoTune.
