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

## Optional Siemens S7 client

When a Siemens OPC UA runtime license is unavailable, MotionBridge can use the
free Snap7 client over Siemens ISO-on-TCP port 102. The default offline build
remains dependency-free; enable this adapter explicitly:

```sh
cmake -S . -B build-s7 -DMOTIONBRIDGE_ENABLE_S7=ON
cmake --build build-s7 --config Release
```

The dependency is pinned to a specific Snap7 revision. On Windows, CMake copies
the required `snap7.dll` beside the probe executable. The initial probe is
deliberately read-only:

```powershell
.\build-s7\motionbridge_s7_probe.exe 192.168.10.1 2
```

For a multi-configuration Visual Studio build, use
`.\build-s7\Release\motionbridge_s7_probe.exe` instead.

The current Siemens data contract is a non-optimized global `DB2`:

| Byte offset | PLC variable | Siemens type | Direction |
|---:|---|---|---|
| 0 | `ControlWord` | `WORD` | PLC → C++ |
| 2 | `TargetPosition` | `LREAL` | PLC → C++ |
| 10 | `MaxVelocity` | `LREAL` | PLC → C++ |
| 18 | `MaxAcceleration` | `LREAL` | PLC → C++ |
| 26 | `HeartbeatPLC` | `UDINT` | PLC → C++ |
| 30 | `StatusWord` | `WORD` | C++ → PLC |
| 32 | `ActualPosition` | `LREAL` | C++ → PLC |
| 40 | `ActualVelocity` | `LREAL` | C++ → PLC |
| 48 | `FollowingError` | `LREAL` | C++ → PLC |
| 56 | `FaultCode` | `UINT` | C++ → PLC |
| 58 | `HeartbeatController` | `UDINT` | C++ → PLC |

The PLC must permit PUT/GET communication, the DB must have optimized block
access disabled, and TCP port 102 must be reachable. The codec has automated
big-endian tests for the exact 62-byte DB layout. Never expose an unprotected
S7 endpoint to an untrusted network.

### Validated PLCSIM Advanced result

The live demo has been verified against an S7-PLCSIM Advanced S7-1500 instance
at `192.168.10.1`. A PLC command moved the C++ simulated axis to its target and
the controller returned status to DB2:

```text
Final state:          RUNNING
Final position:       1.000 rad
Following error:      -0.000 rad
PLC reads:            306
PLC writes:           301
Communication errors: 0
```

`RUNNING` is expected in this commissioning result because OB1 deliberately
holds the Start bit high. Production sequencing will use a pulse or
sequence/acknowledgement handshake.

## Milestone status

![Tests](https://img.shields.io/badge/tests-13%2F13%20passing-brightgreen)
![Control kernel](https://img.shields.io/badge/control%20kernel-complete-brightgreen)
![Siemens network](https://img.shields.io/badge/Siemens%20network-complete-brightgreen)
![ROS 2](https://img.shields.io/badge/ROS%202-planned-blue)
![EtherCAT](https://img.shields.io/badge/EtherCAT-planned-blue)

| Capability | Status | Evidence |
|---|---|---|
| C++20 motion-control kernel | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | State machine, trajectory, PID, plant, and fixed-period loop |
| Automated verification | ![Passing](https://img.shields.io/badge/status-passing-brightgreen) | 13 unit and integration tests |
| Visual telemetry | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | CSV logging and dependency-free SVG plots |
| PLC contract and safety demo | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | Control/status words, heartbeat, watchdog, fault recovery |
| Asynchronous PLC communication | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | 50 Hz worker and latest-value mailboxes around the 1 kHz loop |
| Real OPC UA client boundary | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | Optional pinned `open62541` adapter and connection probe |
| Siemens PLCSIM Advanced network | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | Dedicated virtual adapter and reachable S7 port 102 |
| Siemens S7 client boundary | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | Snap7 adapter, exact DB2 codec, and read-only probe |
| Live S7 runtime integration | ![Complete](https://img.shields.io/badge/status-complete-brightgreen) | PLC command moved the axis; 306 reads, 301 writes, zero communication errors |
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

1. Replace the commissioning Start override with a PLC pulse or
   sequence/acknowledgement handshake.
2. Expose the tested kernel through a ROS 2 `ros2_control` hardware plugin.
3. Publish joint state, controller state, faults, and cycle statistics.
4. Add a ROS 2 trajectory command while retaining PLC motion permission.
5. Keep ROS 2 and all network activity outside the 1 kHz controller loop.
