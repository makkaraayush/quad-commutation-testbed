# My Bench Testing Diary

Here's my log of all the testing I've been doing on the bench. 

---

### Sept 25, 2026: Finally got the motors starting together!
Reflashed the board today to the proper `DAKEFPVF405` target. Before doing this, Motors 1 and 2 were starting way later than 3 and 4. I did a full ESC calibration sweep (2000 to 1000) using the Betaflight motor tab. Also got the FlySky receiver setup on the UART port. Now, all four motors start up beautifully at almost the exact same time (~1048µs). Big win for today!

### Testing the Radio Link
Plugged in the LiPo and tested the radio link. Everything is showing up perfectly in the receiver tab – no weird dropped frames or anything. It's super responsive.

### Setting up Switches
Mapped out my switches on the FlySky transmitter. I have AUX 1 set to arm the drone (safety first!), and I put ANGLE mode on the VRB knob so I can test self-leveling on the bench without it going crazy.

### Leveling the Drone
Just put it on my desk, leveled it out, and hit "Calibrate Accelerometer" in Betaflight. Looks perfectly flat on the 3D model now.

### Checking Motor Directions
I spun up each motor to make sure they were turning the right way for my quad-X setup. They all spin right, though Motor 4 felt a little stiff by hand. Might just be a tight screw, but I'll keep an eye on it. Oh, and I finally sketched out a quick diagram for the spin directions and saved it in the schematics folder so I don't forget.

### Tuning the Filters
The 500mm frame with big 10-inch props shakes quite a bit. I lowered the gyro and D-term filter multipliers to 0.8 just to smooth out the noise. This should help keep the ESCs from getting super hot while it tries to correct tiny vibrations.

### Tweaking the PIDs for this beast
These big props and old analog ESCs needed some custom PID tuning. I completely turned off Feedforward so it doesn't twitch on the bench. I also cut the P and D gains in half to stop it from over-correcting, and bumped the I-gain up to compensate. Seems a lot more stable for testing now!

### The Tether Setup
Getting ready for live power testing. Tied the drone down to a massive 15kg dumbbell. I am NOT letting this thing fly across the room again. 

### Investigating Weird Noises
I noticed the motors were creaking a lot at idle. Realized it was the low PWM frequency making the stators vibrate. Also figured out why Motor 4 felt weird earlier – when it's powered, the ESC basically locks up because of a shorted MOSFET acting like a brake. Super annoying, but at least I know what's broken now!

### The Crash
Yeah... I tested it untethered before figuring out the Motor 4 issue. Spun it up a bit, the PID loop went nuts trying to level it, and when I killed the throttle, Motor 4 braked instantly while the others kept spinning. Flipped right into a wall. Two props dead, one cracked. I'm taking a break until my new parts arrive!

### Sept 25, 2026: ESC Repair & Pre-Tuning Setup
**The Repair:**
Successfully swapped out the dead ESC for Motor 4. Instead of re-wiring the whole arm, I just desoldered the power/signal wires from the old board and soldered them straight to the new one.

**Results:**
- Motor 4 powers up beautifully.
- The weird electromagnetic braking (shorted MOSFET) is completely gone.
- Spun it up briefly and confirmed it's rotating in the right direction!

**Next Steps:**
Going to recalibrate all four ESCs together to ensure perfect synchronization, and then it's time to strap it to the anchor and get into the PID tuning!

---

### Sept 25, 2026: ESC Calibration & Anchor Prep
**Calibration Success:**
- Recalibrated all four ESCs simultaneously. The synchronization is spot on!
- Set the motor idle up to **7.0%** to ensure reliable, stutter-free startup and smooth commutation across the legacy hardware.

**Next Phase:**
Preparing the 15kg dumbbell anchor setup. It's time to run tethered live-power tuning tests to dial in the PIDs!

---

### Sept 26, 2026: Anchor Evolution for Hover Testing
I've been working on a safe anchor setup using nylon ropes to prevent the drone from flying away or flipping during tuning. Here is how it evolved:

**Step 1: The 15kg Dumbbell**
- I started with a single 15kg dumbbell.
- *Problem:* The dumbbell was too big, creating a high risk of the 10-inch propellers crashing into the weight.
- ![Anchor Step 1](../assets/testbed/anchor_step_1_15kg.jpeg)

**Step 2: Dual 5kg Dumbbells**
- I switched to two 5kg dumbbells, tied to both sides of the drone.
- *Problem:* While better, there was still a slight possibility of the propellers striking them during a hard tilt.
- ![Anchor Step 2](../assets/testbed/anchor_step_2_5kg_split.jpeg)

**Step 3: The Initial Suitcase Platform**
- I placed a suitcase directly under the drone and placed the dumbbells *outside* alongside it. 
- *Result:* This totally eliminated the risk of propeller strikes! When the drone lifts, the anchor allows for a little more than 7 inches of vertical lift. This is the perfect "goldilocks" zone—high enough to test hovering, but low enough to prevent major instability or bad tuning issues from causing a crash.
- ![Anchor Step 3](../assets/testbed/anchor_step_3_suitcase.jpeg)

**Step 4: Bigger Suitcase & Quick-Release Hooks**
- *The Platform:* Switching to a bigger suitcase because the previous one wasn't wide enough for all four arm corners to rest completely flat.
- *Rope Adjustments:* Because this new suitcase is taller/higher, I am increasing the rope length slightly to maintain the optimal vertical travel window.
- *Quick Attachment:* Integrating removable hooks on the tether lines instead of manually tying and untying knots every time—now I can just hook and unhook the corners in seconds.

---

### Sept 26, 2026: Failsafe Testing (Signal Loss)
**Objective:** Test what happens if the transmitter loses connection or runs out of battery.
- **The Test:** Turned on the drone, spooled up the motors slightly, and then completely turned off the transmitter.
- **The Problem:** The motors just kept spinning! The receiver wasn't telling the flight controller to cut the throttle.
- **The Fix:** Configured the failsafe directly on the transmitter. Set Channel 3 (Throttle) to drop to -100%, and Channel 5 (Arming switch) to the disarm position upon signal loss.
- **Result:** Retested. Exactly 1.5 seconds after turning off the remote, the motors completely shut down, perfectly matching the Betaflight failsafe delay configuration. Ready for safe hover testing!
