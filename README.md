# MotionBridge

MotionBridge is a practical C++20 industrial motion-control project. Milestone 1
implements a vendor-independent, single-axis controller that can run without a
PLC, ROS 2 installation, OPC UA server, EtherCAT master, or physical drive.

The first milestone includes:

- industrial controller state machine with latched faults and emergency stop;
- PID controller with saturation, anti-windup, and filtered derivative;
- online trapezoidal position trajectory;
- inertial servo-axis simulation;
- fixed-period control loop with execution-time and deadline statistics;
- command-line demonstration;
- dependency-free unit and integration tests;
- narrow interfaces for future PLC, OPC UA, EtherCAT, and ROS 2 adapters.

## Architecture

```text
Future ROS 2 adapter ──┐
Future PLC/OPC UA ─────┴─> Motion command
                              │
                    Controller state machine
                              │
                    Trapezoidal trajectory
                              │
                         PID controller
                              │
                    Simulated servo plant
                              │
                 Status + timing diagnostics
                              │
Future EtherCAT adapter <─────┘
```

The control kernel has no dependency on any vendor SDK. Later integrations
implement the interfaces in `include/motionbridge/interfaces/`.

## Build

Requirements: a C++20 compiler and CMake 3.20 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Linux or with a single-configuration generator:

```sh
./build/motionbridge_demo --target 1.57 --max-velocity 0.8 --max-acceleration 1.5
```

With Visual Studio on Windows:

```powershell
.\build\Release\motionbridge_demo.exe --target 1.57 --max-velocity 0.8 --max-acceleration 1.5
```

Useful demo options:

```text
--target <rad>              Target position (default: 1.57)
--max-velocity <rad/s>      Trajectory velocity limit (default: 0.8)
--max-acceleration <rad/s2> Trajectory acceleration limit (default: 1.5)
--duration <seconds>        Maximum simulation time (default: 5.0)
--frequency <Hz>            Control frequency (default: 1000)
--csv <path>                Save per-cycle telemetry as CSV
--fast                      Run without real-time sleeping
--help                      Show command help
```

## Create a visual result

Run a simulation and save the per-cycle reference, actual position, velocity,
torque, and following error:

```sh
./build/motionbridge_demo --target 1.57 --max-velocity 0.8 \
  --max-acceleration 1.5 --duration 5 --fast --csv build/motion.csv
python scripts/plot_telemetry.py build/motion.csv --output build/motion.svg
```

Open `build/motion.svg` in a browser. The chart uses only the Python standard
library, so it requires no third-party packages.

## Next milestones

1. Add an `open62541` OPC UA adapter and Siemens PLC command/status mapping.
2. Expose the kernel through a ROS 2 `ros2_control` hardware plugin.
3. Implement fake CiA 402 PDO exchange behind `IFieldbus`.
4. Add a native EtherCAT or TwinCAT ADS backend without changing the kernel.
