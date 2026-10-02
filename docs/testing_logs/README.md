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
| **Test 02** | Upcoming | ~7-inch tether | Custom 500mm PID tuning from CLI dump | Planned | Coming soon |

---

## Folder Layout
* `test_01/` - Video, tuning screenshots, and detailed diary for Test 01.
