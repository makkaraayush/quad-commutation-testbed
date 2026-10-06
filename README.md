# My 500mm Quadcopter Project

Hey there! Welcome to my custom 500mm drone build log and diagnostic testbed. I originally started this out on an old 8-bit APM setup, but lately I decided to move it over to a 32-bit STM32F405 flight controller. This way I could really dive into the low-level PWM signals and test the motor commutation without all the complex autonomous flight checks getting in the way.

## What's on the drone?
* **Frame:** 500mm Diagonal (like a TBS 500)
* **Flight Controller:** DakeFPV F405 (Running STM32F405)
* **Radio:** FlySky FS-i6 Transmitter with FS-iA6B Receiver (using i-BUS, 5-mode mixer)
* **Battery:** 3S 2200mAh LiPo
* **Firmware:** ArduCopter Custom Firmware (V4.7.1, migrated from Betaflight)
* **Motors:** 4x 1000KV sensorless brushless motors (with 1045 propellers)
* **ESCs:** 4x 30A analog ESCs (with phone USB-C boot timing sequence)
* **Navigation:** M10Q 250 GPS + QMC5883L Magnetometer
* **Telemetry & Video (In Progress):** DIY LoRa (433MHz Ra-02) + ESP32-CAM live Wi-Fi video + Bluetooth ground HUD on the FS-i6 (built for under $15 because commercial telemetry and VTX gear are way too expensive!)

## My Notes and Logs
I've been documenting everything as I go, especially since this build fought me quite a bit!
* [The Hardware Story](docs/hardware_architecture.md) - How the build evolved and the headaches I had with STM32 timers and ground loops.
* [My Bench Tests](docs/bench_testing_log.md) - Logs from bench testing, ESC calibration, firmware migration, and flight progression.
* [Crash & Troubleshooting Log](docs/failure_logs.md) - My diary of things that broke, weird glitches, the SCL bus conflict, ESC boot timeouts, and the high-tree rescue mission.
* [Live Testing Logs](docs/testing_logs/README.md) - Chronological flight/bench test runs with video recordings and diary entries (Tests 01 through 08).
* [Hardware Wiring & Circuit Guide](docs/hardware_wiring/README.md) - Exact pinouts, power distribution, ESC connections, and receiver wiring.
* [DIY LoRa Telemetry & Video System Guide](docs/telemetry_and_video/README.md) - Complete blueprint, wiring schematics, and Arduino code for our custom long-range LoRa link, Bluetooth ground HUD, and Wi-Fi video streaming.

## Pics and Media
* **Full Drone:** ![Full Quadcopter](assets/hardware/full_quadcopter_assembly_top_view.jpg)
* **FC Pinout:** ![DakeFPV F405](assets/hardware/dakefpv_f405_pinout_macro.jpg)
* **Wiring Setup:** ![Wiring](assets/hardware/fc_and_receiver_wiring.jpg)
* **RIP Propeller:** ![Cracked Prop](assets/hardware/propeller_crash_damage.jpg)
* **Motor Spin Diagram:** ![Spin Directions](assets/schematics/motor_spinning_directions.jpeg)
* **Video:** [Bench Test Walkaround](assets/media/bench_test_walkaround.mp4)
* **Test 01 Video:** [Test 01 Strapped Hardware Check](docs/testing_logs/test_01/test_01_hardware_check_strapped.mp4)
* **Test 02 Video:** [Test 02 Untethered Takeoff Wobble & Abort](docs/testing_logs/test_02/test_02_takeoff_wobble_abort.mp4)
* **Test 03 Video:** [Test 03 Stable Hover Flight](docs/testing_logs/test_03/test_03_stable_hover.mp4)
* **Test 04 Video:** [Test 04 In-Flight Failsafe Drop Test](docs/testing_logs/test_04/test_04_failsafe_check.mp4)
* **Test 05 Video:** [Test 05 ArduPilot Arming Check](docs/testing_logs/test_05/test_05_arm_test.mp4)
* **Test 06 Video:** [Test 06 ArduPilot Stabilization Flight](docs/testing_logs/test_06/test_06_stabilization_test.mp4)
* **Test 07 Video:** [Test 07 ArduPilot Alt-Hold Altitude Lock](docs/testing_logs/test_07/test_07_alt_hold_test.mp4)
* **Test 08 Log:** [Test 08 Outdoor RTL Flights & Tree Rescue](docs/testing_logs/test_08/README.md)

