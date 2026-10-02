# Quadcopter Commutation & Flight Testing Logs

Welcome to the live test repository for the custom 500mm quadcopter testbed. Every live power run, tethered evaluation, and tuning flight is logged systematically with video recordings, configuration snapshots, and diagnostic telemetry.

---

## 🛡️ Testing Methodology & Safety Protocols

Because this build uses a large 500mm wheelbase and powerful 10-inch propellers, testing follows a strict phased safety protocol to prevent runaway flyaways, frame damage, or injury:

1. **Phase 1: Strapped Hardware Validation (Zero Travel)**
   * Drone is tied down flat to the suitcase anchor base using heavy-duty straps.
   * Tests motor direction, commutation under load, ESC synchronization, and disarm/failsafe responsiveness without any flight hazard.
2. **Phase 2: Constrained Tethered Hover (~7-Inch Vertical Travel)**
   * Drone is attached to corner tethers anchored to external weights.
   * Allows enough lift to observe self-leveling and roll/pitch stability in the "goldilocks" zone while preventing dangerous pitch-ups or tip-overs.
3. **Phase 3: Expanded Tether Envelope**
   * Testing altitude hold and yaw authority with extended tether margins.
4. **Phase 4: Free Flight**
   * Outdoor untethered flight after full PID stabilization.

---

## 📑 Test Log Index

| Test Run | Date | Configuration | Testbed Setup | Status | Detailed Report |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Test 01** | 2026-10-03 | Stock Betaflight 5" PIDs | Fully Strapped to Suitcase | **PASSED** (Hardware 100% verified; PIDs too aggressive for 500mm) | [View Test 01 Log](test_01/README.md) |
| **Test 02** | *Upcoming* | Custom 500mm Low-Gain PIDs (`cli_dump.txt`) | Constrained Tether (~7" lift) | *Planned* | TBD |

---

## 📁 Directory Structure

```text
testing_logs/
├── README.md                          # Testing index and protocol overview (this file)
└── test_01/                           # Test 01 session files
    ├── README.md                      # Detailed log, whiteboard transcription, and analysis
    ├── test_01_hardware_check_strapped.mp4 # Video of live strapped test
    └── test_01_stock_pid_preset.png   # Screenshot of Betaflight PID profile used
```
