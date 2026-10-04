# My 500mm Quadcopter Project

Hey there! Welcome to my custom 500mm drone build log and diagnostic testbed. I originally started this out on an old 8-bit APM setup, but lately I decided to move it over to a 32-bit STM32F405 flight controller. This way I could really dive into the low-level PWM signals and test the motor commutation without all the complex autonomous flight checks getting in the way.

## What's on the drone?
* **Frame:** 500mm Diagonal (like a TBS 500)
* **Flight Controller:** DakeFPV F405 (Running STM32F405)
* **Radio:** FlySky FS-i6 Transmitter with FS-iA6B Receiver (using i-BUS)
* **Battery:** 3S LiPo
* **Software:** Betaflight 4.4+ (Flashed as DAKEFPVF405)
* **Motors:** 4x 1000KV sensorless brushless motors
* **ESCs:** Some older 30A analog ESCs (I had to mod them to fix some power issues!)

## My Notes and Logs
I've been documenting everything as I go, especially since this build fought me quite a bit!
* [The Hardware Story](docs/hardware_architecture.md) - How the build evolved and the headaches I had with STM32 timers and ground loops.
* [My Bench Tests](docs/bench_testing_log.md) - Logs from when I was testing it on the bench, tuning the ESCs and fixing motor start issues.
* [Crash & Troubleshooting Log](docs/failure_logs.md) - My diary of things that broke, weird glitches, and that time it randomly freaked out on the bench.
* [Live Testing Logs](docs/testing_logs/README.md) - Chronological flight/bench test runs with video recordings and configuration profiles (Test 01+).
* [Hardware Wiring & Circuit Guide](docs/hardware_wiring/README.md) - Exact pinouts, power distribution, ESC connections, and receiver wiring.

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
