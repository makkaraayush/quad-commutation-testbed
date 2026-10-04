# What Broke & How I Fixed It

This is where I'm keeping track of all the weird issues I ran into during the upgrade, so I don't go crazy trying to remember how I fixed them!

### Issue 1: Two motors just wouldn't spin
- **What happened:** Motors 1 and 2 worked fine, but 3 and 4 were completely dead.
- **The fix:** Turned out there was a timer collision because the STM32F405 was trying to run DSHOT. I just switched it back to legacy analog PWM (400Hz) and they woke up!

### Issue 2: Radio stopped talking to the drone
- **What happened:** After a firmware flash, I completely lost my FlySky IBUS connection.
- **The fix:** I accidentally wiped the UART settings. I just had to go back into Betaflight and reassign UART2 to Serial RX.

### Issue 3: Flashed the wrong firmware (oops)
- **What happened:** I accidentally flashed the `SPEEDYBEEF405V3` target instead of `DAKEFPVF405`. 
- **The fix:** Re-flashed it with the right target, recalibrated all the ESCs together (2000µs to 1000µs), and now all four motors start up perfectly at the same time. 

### Issue 4: Motors were making a weird creaking sound
- **What happened:** When armed at idle, all the motors were making this weird groaning/creaking noise.
- **The fix:** The low PWM frequency (400Hz) was causing some stator chatter. I raised the idle throttle just a tiny bit (to 1065) and bumped the PWM rate to 480Hz. It runs super smooth now!

### Issue 5: The cursed Motor 4 (Shorted ESC)
- **What happened:** Motor 4 would spin freely when off, but as soon as I plugged the battery in and dropped throttle, it felt like it had the brakes slammed on. 
- **The fix:** Confirmed it was a shorted MOSFET in the ESC that was acting like an electromagnetic brake. 

### Issue 6: The Bench Incident (RIP Propellers)
- **What happened:** I was testing on the bench (not tied down) in ANGLE mode. I gave it a tiny bit of throttle, the PID loop freaked out (windup), and when I dropped the throttle, Airmode kept trying to balance the drone. Because Motor 4's ESC was braking hard, it pitched violently, flew into the wall, and destroyed my propellers.
- **The fix:** Lesson learned: **ALWAYS tie it down to the 15kg dumbbell anchor when testing with props on!**

### Crash Repair Update!
**Status: Success!**
The new soldering strategy worked perfectly. I desoldered the wires from the old ESC board and connected them directly to the new one without taking the whole frame apart. 
- The new ESC is running flawlessly.
- No more electromagnetic braking/shorting issues on Motor 4!
- The motor is spinning in the correct direction.

Next up: Calibrating all the ESCs together and starting the tuning process!

### Issue 7: Failsafe Failed (Motors Kept Spinning on Signal Loss)
- **What happened:** I did a test where I turned off the remote to see if the drone would disarm. Instead, the motors kept right on spinning! Total flyaway risk.
- **The fix:** I had to go into the transmitter's own menu and explicitly turn on the failsafe. I set Channel 3 (throttle) to -100% and Channel 5 (arming switch) to the disarm position. Now, it correctly disarms exactly 1.5 seconds after signal loss (which matches Betaflight's config).

### Issue 8: Betaflight Configurator Failed to Flash ArduPilot Hex
- **What happened:** Tried to flash `arducopter_with_bl.hex` using Betaflight Configurator's "Load Firmware [Local]" tab, but the flash process kept erroring out and refusing to write. Betaflight's flasher is designed specifically for Betaflight firmware and balks at external bootloaders and full-chip hex layouts.
- **The fix:** Switched to **STM32CubeProgrammer**. Booted the STM32F405 into DFU mode, loaded the hex, and flashed it with a full chip erase. It wrote and verified cleanly on the first try.

### Issue 9: M10Q-5883 Compass Undetected (SCL Voltage Drop / LED_TYPE Bus Conflict)
- **What happened:** After installing ArduPilot and wiring up the M10Q-5883 unit, the GPS locked satellites and worked fine, but the compass was completely missing in Mission Planner. Tweaked all compass parameters (`COMPASS_ENABLE`, `COMPASS_AUTO_ROT`, bus IDs), but nothing brought it to life.
- **Diagnostics with Multimeter:**
  - Checked resistance across the lines: readings were normal (no dead short to ground).
  - Unpowered behavior: With the GPS plugged into the FC but unpowered, both SDA and SCL floated around ~1.5V.
  - Powered behavior: Once the GPS was supplied with 5V, SDA immediately pulled up to a healthy 3.3V, but SCL remained stuck down (around 0.2V).
  - The Isolation Test: Desoldered the SCL wire from the FC pad. The loose SCL wire from the GPS immediately jumped up to a solid **3.3V**, and the FC pad showed zero shorts to ground. This proved the GPS module, pull-up resistors, and wiring harness were 100% healthy—the issue was purely inside ArduPilot!
- **The Root Cause:** Parameter collision! `NTF_LED_TYPES` (or `LED_TYPE`) was set to `455`, which instructed ArduPilot to initialize external I2C LED drivers on boot. Because no external I2C LED hardware was connected, the driver grabbed and locked up the I2C bus, dragging the SCL clock line down to 0.2V.
- **The Fix:** Changed the parameter to **1** (Built-in / Internal Board LEDs only). Rebooted the board, the I2C bus was instantly released, SCL snapped up to 3.3V, and the compass was detected immediately in Mission Planner!

### Issue 10: Analog ESCs Refusing to Start on ArduPilot Boot (The 8-Hour Mystery)
- **What happened:** After flashing ArduPilot, plugging in the LiPo battery resulted in completely dead/silent ESCs. They refused to arm or play their startup beeps because they weren't receiving the initial zero-throttle PWM signal on power-up. Yet, strangely enough, if the flight controller was already plugged in and powered via USB first, plugging in the LiPo afterwards made all four ESCs start and run flawlessly!
- **The 8-Hour Grind:** Spent 8 hours nonstop changing every parameter imaginable in Mission Planner, rewiring connections, testing solder joints, and pulling hair out trying to understand why it worked with USB but failed with standalone battery power.
- **The Mobile Charger Breakthrough:** To test a theory about boot timing, I used a standard mobile phone charger to power the FC's 5V input first. I waited a few seconds for ArduPilot to finish its full startup sequence, and then plugged in the LiPo to power the ESCs. Boom—all four ESCs immediately sang their initialization chimes and armed perfectly!
- **The Root Cause:** Hardware timer mismatch between legacy analog ESCs and ArduPilot's bootloader:
  - These older 30A analog ESCs have a strict hardware timeout window on boot. If they don't detect a valid PWM throttle signal within a couple seconds of receiving battery power, their firmware assumes a signal fault and enters a protective lockout state to prevent runaway props.
  - Betaflight never had this issue because it is an ultra-lightweight firmware that boots in milliseconds, immediately spitting out 480Hz PWM pulses before the ESC timeout expires.
  - ArduPilot, on the other hand, runs a full ChibiOS real-time operating system with extensive pre-arm checks, sensor initialization, and EKF filtering that takes roughly 4 to 8 seconds to boot before enabling motor timer outputs.
  - When plugging in the LiPo directly, the ESCs wake up, wait for a signal that isn't ready yet, time out, and shut down before ArduPilot ever finishes booting!
- **The Solution / Workaround:** Power the flight controller a few seconds before the ESCs (or use an auxiliary pre-boot step), giving ArduPilot time to boot and start driving its PWM signal lines before the ESCs run their startup checks.




