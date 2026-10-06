# My Flight & Bench Testing Logs

Welcome to my live testing logs! This is where I document all my real power-on tests, tethered hover sessions, and eventual flight tests as I dial in this custom 500mm quad.

---

## How I'm Approaching Testing Safely
A 500mm drone with 10-inch props can do serious damage if something goes wrong, so I'm taking things in careful, calculated steps rather than rushing outside and hoping for the best:

1. **Phase 1: Strapped Down Hardware Check**
   The drone is completely tied down flat to my suitcase testbed. This lets me check motor spin directions, test ESC synchronization under real battery load, and make sure the arm/disarm switches and failsafes work without any risk of the drone flipping into a wall.
2. **Phase 2: Constrained Tethered Hover (~7 Inches of Lift)**
   The drone is attached to corner lines tied to weights outside the propeller radius. This gives it just enough slack (about 7 inches) to lift off and test whether the PID loop can self-level, while keeping it low enough that a bad oscillation won't cause a violent crash.
3. **Phase 3: Looser Tether / Higher Hover**
   Once hover stability is dialed in, I'll extend the lines to check yaw control and altitude stability.
4. **Phase 4: Free Flight**
   Real untethered flying once everything is completely proven on the bench rig.

---

## The Test Logs

| Test | Date | Setup | What was tested | Outcome | Link |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Test 01** | Oct 03, 2026 | Strapped to suitcase | Arming, motor sync, and stock PID behavior | Hardware passed; stock 5-inch PIDs are way too aggressive | [Read Test 01 Log](test_01/README.md) |
| **Test 02** | Oct 03, 2026 | Untethered on floor | First liftoff attempt with custom low-gain PIDs | Aborted; extreme wobble on liftoff, cut throttle for safety | [Read Test 02 Log](test_02/README.md) |
| **Test 03** | Oct 04, 2026 | Untethered on floor | Fine-tuned hover test (wobble elimination) | Passed; wobble completely gone, rock-solid hover achieved | [Read Test 03 Log](test_03/README.md) |
| **Test 04** | Oct 04, 2026 | Untethered hover | In-flight failsafe motor cut verification | Passed; motors cut cleanly, quad dropped safely | [Read Test 04 Log](test_04/README.md) |
| **Test 05** | Oct 06, 2026 | Untethered on floor | ArduPilot arming and idle motor synchronization | Passed; clean arm, synchronous idle spin, instant disarm | [Read Test 05 Log](test_05/README.md) |
| **Test 06** | Oct 06, 2026 | Untethered indoor flight | ArduPilot Stabilize mode flight & attitude control | Passed; smooth liftoff, rock-solid stabilization, zero wobble | [Read Test 06 Log](test_06/README.md) |
| **Test 07** | Oct 06, 2026 | Untethered indoor flight | ArduPilot Alt-Hold automated altitude lock | Passed; locked vertical hold via baro/EKF, hands-off throttle | [Read Test 07 Log](test_07/README.md) |
| **Test 08** | Oct 06, 2026 | Outdoor open field | ArduPilot Return-To-Launch (RTL) & failsafe behavior | Passed RTL twice (15m climb, exact return); low-batt RTL tree snag rescued via custom pole & diving catch | [Read Test 08 Log](test_08/README.md) |

*Note: Tests 01 to 04 were conducted under Betaflight during initial hardware and filter validation. Tests 05 to 07 established indoor baseline health and altitude hold on ArduPilot. Test 08 marked the first outdoor autonomous GPS navigation flights and Return-To-Launch validation.*

---

## Folder Layout
* `test_01/` - Video, tuning screenshots, and detailed diary for Test 01.
* `test_02/` - Video, tuning screenshots, and post-mortem on the Test 02 wobble.
* `test_03/` - Video, tuning screenshot, and analysis of the successful hover flight.
* `test_04/` - Video and analysis of the in-flight failsafe drop test.
* `test_05/` - Video and analysis of the ArduPilot arming and ESC synchronization check.
* `test_06/` - Video and analysis of the ArduPilot manual attitude stabilization flight.
* `test_07/` - Video and analysis of the ArduPilot Alt-Hold vertical altitude lock test.
* `test_08/` - Diary and analysis of the outdoor RTL trials, low-battery failsafe tree snag, and barehanded rescue catch.



