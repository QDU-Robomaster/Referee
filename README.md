# Referee

RoboMaster referee system (2025 protocol) receiver and sender over UART.

## Behaviour

The constructor configures the UART (`baudrate`, 8N1), creates the output topics and
starts the `Referee` thread (stack `task_stack_depth_uart`, priority
`thread_priority_uart`). The thread loops:

1. Read byte by byte until the `0xA5` start of frame, read the rest of the header and
   check its CRC8. A failed UART read marks the link `OFFLINE`; a valid header marks it
   `RUNNING`.
2. Read the command ID, payload and tail, check the frame CRC16 and copy the payload
   into the matching field of the internal data set (0x0001-0x0310 robot and game data,
   0x0A01-0x0A06 radar link data, 0x0F01/0x0F02 video channel replies). Frames with an
   unknown command ID or a short payload are ignored.
3. If the frame was parsed, publish the topics below.
4. Sleep 10 ms.

Published topics (names are configurable, all created as multi-publisher topics):

| Default name | Type | Content |
|---|---|---|
| `chassis_ref` | `Referee::ChassisPack` | `RobotStatus` (level, power limit, ...) and chassis power buffer (J) |
| `launcher_ref` | `Referee::LauncherPack` | `RobotStatus`, `RobotBuff`, `LauncherData`, `DartClient`, `PowerHeat` |
| `robot_game_ref` | `Referee::RobotGameRefereePack` | robot status, game status, sentry info, RFID, 17 mm allowance, outpost/base HP, robot positions |
| `radar_ref` | `Referee::RadarPack` | topic is created but currently not published |

Keyboard and mouse data from the video link (0x0304) is forwarded to `CMD::FeedRC()`
when a `CMD` is bound: W/A/S/D set the chassis x/y command to ±0.5 (doubled with Shift),
mouse x/y drive gimbal yaw/pitch (scale 1000/32768), the left button fires, and the
control source is `CTRL_SOURCE_RC`. With `cmd = nullptr` this forwarding is disabled;
`BindCMD(CMD&)` can bind one later.

Sending (all frames get header CRC8, frame CRC16 and a sequence number; writes are
serialized by a mutex):

- `SendFrame(cmd_id, payload)` and `SendStudentCmd(data_cmd_id, sender, receiver, payload)`
  (robot interaction frames, 0x0301).
- Client UI: `FillLine`, `FillRect`, `FillCircle`, `FillEllipse`, `FillArc`, `FillFloat`,
  `FillInt`, `FillCharacter` build figures; `SendUIFigure`, `SendUIFigure2/5/7`,
  `SendUICharacter`, `SendUILayerDelete` send them. `GetRobotID()` and
  `GetClientID(robot_id)` give the sender and receiver IDs.
- Sentry decisions: `SetNeedBullet`, `SetConfirmRevival`, `SetBulletRemote`,
  `SetHPRemote`, `SetRevivalRemote`, `SetSwitchMode` update the stored 0x0120 payload;
  `SendSentryPack()` sends it to the referee server. `SendSentryDecision`,
  `SendRadarDecision`, `SendRadarPack` send explicit payloads.
- Video link: `SendSetVideoTransChannel(channel)`, `SendQueryVideoTransChannel()`,
  `SendCustomDataToController(...)`.

## Shared message types

`RefereeTypes.hpp` provides the producer-owned `RobotGameRefereePack` and its
component types (`GameStatus`, `RobotStatus`, `RobotPOS`, `RFID`, `RobotPosForSentry`,
`SentryInfo`) without including CMD, UART or the parser. Host subscribers can include
this header directly. `Referee::RobotGameRefereePack` and the component names inside
`Referee` are aliases to these same types. The packed layout is 92 bytes.

## Dependencies

- `QDU-Robomaster/CMD`: receives the video-link keyboard/mouse control through
  `CMD::FeedRC()`.

No external packages.

## Constructor

```cpp
Referee(LibXR::UART& uart,
        CMD* cmd,
        const Param& param = {.task_stack_depth_uart = 2048, .baudrate = 115200,
                              .referee_chassis_tp_name = "chassis_ref",
                              .referee_launcher_tp_name = "launcher_ref",
                              .referee_robot_game_tp_name = "robot_game_ref",
                              .referee_radar_tp_name = "radar_ref",
                              .thread_priority_uart = LibXR::Thread::Priority::LOW});
```

Dependencies:

- `uart`: `LibXR::UART` connected to the referee system (or video link).
- `cmd`: pointer to a `CMD` instance that receives video-link keyboard/mouse control,
  or `nullptr`.

Configuration (`Param`):

- `task_stack_depth_uart`: stack depth of the `Referee` thread, default 2048.
- `baudrate`: UART baud rate, default 115200.
- `referee_chassis_tp_name`: chassis topic name, default `"chassis_ref"`.
- `referee_launcher_tp_name`: launcher topic name, default `"launcher_ref"`.
- `referee_robot_game_tp_name`: summary topic name, default `"robot_game_ref"`.
- `referee_radar_tp_name`: radar topic name, default `"radar_ref"`.
- `thread_priority_uart`: thread priority, default `LibXR::Thread::Priority::LOW`.

## Use

```sh
xrobot module add QDU-Robomaster/Referee
xrobot setup
xrobot instance add QDU-Robomaster/Referee
```

`xrobot instance add` writes an instance to `User/xrobot.yaml` with empty dependencies
and the source defaults; fill in the dependencies:

```yaml
modules:
  - module: QDU-Robomaster/Referee
    id: referee_0
    args:
      - uart: usart1
      - cmd: cmd
      - param:
          task_stack_depth_uart: '2048'
          baudrate: '115200'
          referee_chassis_tp_name: '"chassis_ref"'
          referee_launcher_tp_name: '"launcher_ref"'
          referee_robot_game_tp_name: '"robot_game_ref"'
          referee_radar_tp_name: '"radar_ref"'
          thread_priority_uart: LibXR::Thread::Priority::LOW
```

BSP side:

```cpp
XR_REGISTER(usart1, LibXR::UART);
```

`cmd` is the `id` of a `QDU-Robomaster/CMD` instance and must be listed earlier in
`modules:`; write `cmd: nullptr` to run without keyboard/mouse forwarding. Modules that
subscribe to the Referee topics at construction (for example `SuperPower`,
`SentryProtocol`) must be listed after this instance.

Run `xrobot setup` again to generate `User/xrobot_main.hpp`.

`xrobot module show .` in this repository, or
`xrobot module show Modules/QDU-Robomaster/Referee` in a BSP, prints the current
constructor.

## Test

With `BUILD_TESTING` enabled in the build that includes this Module, the
`referee_types_test` target checks the `RobotGameRefereePack` size and field offsets at
compile time; run it with `ctest -R referee_types_test`.
