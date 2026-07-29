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

The PLC-supervision increment also includes:

- industrial control-word and status-word mapping;
- separate 50 Hz PLC and 1 kHz control timing domains;
- PLC and controller heartbeat counters;
- communication watchdog with a latched timeout fault;
- a mock PLC used for local demonstrations and automated tests.

## Architecture

```text
ROS 2 adapter (planned) ----+
PLC / OPC UA ---------------+--> Motion command
                                  |
                           State machine
                                  |
                        Trajectory generator
                                  |
                            PID controller
                                  |
                  Fieldbus interface / servo plant
                                  |
                     Status + timing diagnostics
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

## Run the PLC-supervised safety demo

This local demo needs no Siemens or TwinCAT installation. It uses the same PLC
interface that a future OPC UA adapter will implement:

```sh
./build/motionbridge_plc_demo
```

The mock PLC powers and enables the axis, starts a move, freezes its heartbeat
to simulate a network failure, and then reconnects and explicitly resets the
latched controller fault. Expected state sequence:

```text
POWER_OFF -> DISABLED -> READY -> RUNNING
RUNNING -> FAULT (COMMUNICATION_TIMEOUT)
FAULT -> DISABLED -> READY
```

### Asynchronous communication demo

The production-oriented path keeps PLC or OPC UA work off the 1 kHz control
thread. A `PlcCommunicationWorker` performs blocking reads and writes at 50 Hz.
The controller exchanges only the newest command and status through bounded,
non-blocking mailboxes:

```text
PLC / OPC UA (50 Hz worker thread)
              |
       latest-value mailboxes
              |
       control loop (1 kHz)
```

Run the local threaded demonstration:

```sh
./build/motionbridge_async_plc_demo
```

It executes 1,500 control cycles while performing roughly 75 PLC reads. A
frozen heartbeat still causes the control-side watchdog to latch a communication
fault, after which heartbeat recovery and an explicit reset return the axis to
`READY`.

## Optional real OPC UA client

The real PLC adapter uses `open62541` and is excluded from the default offline
build. Enabling it downloads the pinned `v1.5.4` SDK and builds a connection
probe:

```sh
cmake -S . -B build-opcua -DMOTIONBRIDGE_ENABLE_OPCUA=ON
cmake --build build-opcua --config Release
```

Copy `config/opcua_nodes.example.conf`, then replace its endpoint, namespace,
and string NodeIds with the exact values exposed by the PLC server. Check the
connection and read the command block:

```sh
./build-opcua/motionbridge_opcua_probe config/opcua_nodes.conf
```

The probe intentionally only connects and reads. OPC UA network calls are not
placed inside the 1 kHz control loop; the later runtime integration will use a
separate communication thread and a bounded mailbox.

This first probe uses an anonymous session with OPC UA SecurityMode `None`.
That is suitable for local commissioning only. Certificate trust, signed and
encrypted sessions, and PLC access-control configuration are required before
using the adapter on a production network.

## Milestone status

![Tests](https://img.shields.io/badge/tests-12%2F12%20passing-brightgreen)
![Control kernel](https://img.shields.io/badge/control%20kernel-complete-brightgreen)
![Siemens integration](https://img.shields.io/badge/Siemens%20integration-awaiting-yellow)
![ROS 2](https://img.shields.io/badge/ROS%202-planned-blue)
![EtherCAT](https://img.shields.io/badge/EtherCAT-planned-blue)

| Capability | Status | Evidence |
|---|---|---|
| C++20 motion-control kernel | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | State machine, trajectory, PID, plant, and fixed-period loop |
| Automated verification | ![Passing](https://img.shields.io/badge/status-passing-brightgreen) | 12 unit and integration tests |
| Visual telemetry | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | CSV logging and dependency-free SVG plots |
| PLC contract and safety demo | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | Control/status words, heartbeat, watchdog, fault recovery |
| Asynchronous PLC communication | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | 50 Hz worker and latest-value mailboxes around the 1 kHz loop |
| Real OPC UA client boundary | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | Optional pinned `open62541` adapter and connection probe |
| Siemens PLCSIM Advanced connection | ![Awaiting](https://img.shields.io/badge/status-awaiting-yellow) | Configure the PLC OPC UA server and exact NodeIds |
| Live OPC UA runtime integration | ![Awaiting](https://img.shields.io/badge/status-awaiting-yellow) | Attach `OpcUaPlcClient` to the communication worker |
| ROS 2 integration | ![Planned](https://img.shields.io/badge/status-planned-blue) | `ros2_control` hardware plugin and telemetry |
| EtherCAT / CiA 402 integration | ![Planned](https://img.shields.io/badge/status-planned-blue) | Fake PDO backend first, then a native backend |

Status legend:
![Complete](https://img.shields.io/badge/complete-passing-brightgreen)
![Awaiting](https://img.shields.io/badge/awaiting-next-yellow)
![Planned](https://img.shields.io/badge/planned-backlog-blue)
![Blocked](https://img.shields.io/badge/blocked-failing-red)

The project is a functional controller simulation, not a safety-certified
controller or a hard-real-time Windows application. Timing statistics describe
the measured demo environment only.

## Immediate next steps

1. Create the MotionBridge command and status data block in TIA Portal.
2. Enable the OPC UA server in the Siemens PLC or PLCSIM Advanced instance.
3. Record the endpoint, namespace index, and exact NodeIds in
   `config/opcua_nodes.conf`.
4. Run `motionbridge_opcua_probe` to prove the real connection and reads.
5. Connect `OpcUaPlcClient` to `PlcCommunicationWorker`, keeping all network
   activity outside the 1 kHz controller loop.
