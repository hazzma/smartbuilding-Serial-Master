# RS485 Agent Documentation Report

> Current alignment note: the normative architecture document is `docs/Smart_Building_RS485_Modbus_Architecture.md`; the normative slave wire contract is `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md` v2.1.0. Older implementation notes below describe intermediate firmware phases and should be treated as historical unless they match those normative documents.

## Reference

Primary reference:

- `docs/Smart_Building_RS485_Modbus_Architecture.md`

Legacy phase reference:

The Phase 1 bullets below describe an earlier implementation pass. They are retained for history only and are superseded by the v2.1.0 contract named above.

- RS485 transport uses Modbus RTU.
- Master owns the bus.
- Slave only responds to master polling/read/write.
- Pairing/default address is `247`.
- Identity follows the slave contract: `DEVICE_MAGIC` at `0x0000`, `FW_VERSION` at `0x0005`, and MAC registers at `0x0006..0x0008`.
- Capability count registers are `0x0011..0x0016`.
- Runtime polling follows the slave contract register groups: temperature `0x0100..0x0103`, air quality `0x0110..0x0112`, presence `0x0120..0x0121`, and relay `0x0130..0x0131`.
- Slave Detail checkbox state is master-owned; SAVE writes capability counts to `0x0011..0x0016`, then may write `0x00F1 = 0xA55A`.

## Phase 1 - Modbus Transport Skeleton

Status: Implemented and build-passed, pending hardware validation.

### Changed Files

- `platformio.ini`
- `src/rs485_manager.h`
- `src/rs485_manager.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Added `DFRobot_RTU` dependency as the Modbus RTU library required by the architecture document.
- Reworked `rs485_manager.cpp` from custom binary frame transport into a Modbus RTU master backend.
- Kept the existing public RS485 manager API stable so current UI callbacks, task creation, and state flags remain compatible.
- Added Modbus register constants to `rs485_manager.h` for identity, capability counts, sensor groups, presence, and relay registers.
- Preserved `Task_RS485` on Core 0 so WiFi, LAN, MQTT, UI, and touch code do not need changes in Phase 1.
- Preserved Serial debug commands for no-slave testing.
- Updated master MAX3485 direction control to PCB `COM_SW` on GPIO21.

### Phase 1 Command Mapping

The existing UI commands are mapped to Modbus register operations:

| UI / Legacy Command | Modbus Operation |
|---|---|
| `RS485_CMD_PING` | Read holding register `0x0000` (`DEVICE_MAGIC`) |
| `RS485_CMD_GET_INFO` | Read identity registers including `0x0000`, `0x0005`, and MAC `0x0006..0x0008` |
| `RS485_CMD_READ_SENSOR` | Read runtime groups from `0x0100`, `0x0110`, `0x0120`, and relay state as needed |
| `RS485_CMD_GET_CONFIG` | Read capability count registers `0x0011..0x0016` |
| `RS485_CMD_SET_OUTPUT` | Write holding register `0x0130` (`RELAY_1_STATE`) |

### Serial Monitor Test Commands

Baud rate: `115200`

Use these without real slave hardware:

```text
rs485 stats
rs485 inject ok
rs485 test 0x10 0x03
rs485 inject timeout
rs485 test 0x10 0x03
rs485 inject crc
rs485 test 0x10 0x03
rs485 pair 5000
rs485 stats
```

Use these with Modbus slave hardware:

```text
rs485 poll off
rs485 test 0x10 0x01
rs485 test 0x10 0x02
rs485 test 0x10 0x03
rs485 testwrite 0x10
rs485 stats
```

Expected behavior:

- `rs485 test 0x10 0x01` reads `DEVICE_MAGIC`.
- `rs485 test 0x10 0x02` reads identity data including MAC registers.
- `rs485 test 0x10 0x03` reads sensor groups according to enabled capability counts.
- `rs485 testwrite 0x10` writes `1` to `RELAY_1_STATE` at `0x0130`.
- `rs485 inject ...` forces the next transaction result so counters and UI status can be tested without slave hardware.

### Deliberately Not Implemented Yet

- Dedicated per-channel light dashboard model.
- Dedicated AC/projector command routing beyond local desired-state UI.
- Slave-side duplicate WRITE protection.

### Notes / Ambiguities From Reference

- The architecture document lists `2-247` as normal slaves and also `247` as pairing/default address. Phase 1 treats `247` as reserved for pairing, so normal discovered slave addresses should avoid `247`.
- `SAVE_CONFIG` is `0x00F1` with value `0xA55A`.
- Phase 1 assumes holding register reads (`0x03`) for all listed architecture registers. If slave firmware exposes sensor values as input registers (`0x04`), the manager should add a per-register function-code map in Phase 2.

## Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware result: pending.

## Dashboard Mapping Rework Phase 5/6 - Compact Persistence and Name Edit

Status: Implemented, build-passed, and flashed.

### Reference Alignment

This compact pass follows the persistence and configuration intent from
`docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`:

- Master persists slave registry.
- Master persists user-facing slave names.
- Master persists enabled capability mask.
- Master persists dashboard mapping and manual override flags.
- Slave name editing uses the full-screen keyboard flow.

### Changed Files

- `src/data.h`
- `src/data.cpp`
- `src/rs485_manager.cpp`
- `src/ui_screens.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Added NVS persistence under namespace `rs485cfg`.
- Persisted per-slave fields:
  - address
  - UID
  - MAC
  - name
  - capability mask
  - enabled mask
  - assignment/count fields
- Persisted logical mapping fields:
  - assigned slave UID/address
  - channel
  - assigned flag
  - manual override flag
- Pairing assignment now auto-saves the registry/mapping after a successful address assignment.
- Added Serial command:

```text
rs485 save
```

- Added compact name edit flow:
  - tap row once to select
  - tap the selected row again to open the full-screen keyboard
  - `ENTER` saves the name to NVS and returns to Slave Manager

### Serial Monitor / UI Test

```text
rs485 pairfake
rs485 stats
rs485 save
```

UI:

```text
Settings -> Slaves
tap row to select
tap same row again
edit name
ENTER
reboot board
```

Expected behavior:

- Edited slave name survives reboot.
- Saved mappings survive reboot.
- Pairing assignment persists automatically after hardware assignment succeeds.

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware flash result: PASS (`pio run -t upload`, auto-detected `COM35`, hard reset via RTS).

## Slave Manager UI Alignment Patch - Pagination and Empty State

Status: Implemented and build-passed.

### Reference Alignment

This patch follows `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`
section 14:

- Slave Manager overview uses pagination.
- Row priority is name, address, status, last seen, then capability summary.
- PING / READ / INFO operate on selected slave.
- Page indicator shows current page and total page count.
- NEXT cycles pages when device count exceeds visible rows.

### Changed Files

- `src/data.cpp`
- `src/rs485_manager.cpp`
- `src/ui_screens.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Default RS485 registry now starts with one empty placeholder device:
  - address `0x00`
  - name `Device 0`
  - zero capability/info
- Slave Manager overview now displays detected online devices only.
- If no slave is detected/online, UI shows one default row:

```text
Device 0  0x00      EMPTY
MAC --  No slave detected
```

- When devices appear online, the overview grows automatically.
- When devices go offline, the overview shrinks back down, with a minimum of one default row.
- Pairing assignment now fills the empty default slot before appending a new row.
- Slave Manager list changed from free-scroll to page layout:
  - 3 rows per page
  - `BACK`
  - `Page x/y`
  - `NEXT >` when needed

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Flash result: not run for this patch.

## Dashboard Mapping Rework Phase 4 - Slave Manager Testing UX

Status: Implemented and build-passed, pending hardware validation.

### Reference Alignment

This phase follows `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md` sections:

- Slave Manager overview should prioritize name, address, status, last seen, and capability summary.
- PING / READ / INFO operate on the selected slave.
- Device list should remain scrollable when it exceeds the viewport.
- Low-level register values should not dominate the overview page.

### Changed Files

- `src/ui_screens.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Expanded Slave Manager rows for better readability:
  - slave name
  - Modbus address
  - online/offline/degraded status
  - last-seen age
  - UID when available
  - MAC when available
  - capability summary including LUX
- Added selected-slave summary in the fixed status area.
- Kept touch scrolling with a proportional scrollbar.
- Kept quick test actions:
  - `PING`
  - `READ`
  - `INFO`
- Added footer guidance showing that quick test buttons target the selected row.

### Serial Monitor / UI Test

Use without slave hardware:

```text
rs485 inject ok
rs485 test 0x10 0x02
rs485 inject ok
rs485 test 0x10 0x05
rs485 stats
```

Expected behavior:

- Slave Manager row shows UID/MAC/capability when debug data is seeded.
- Tapping a row changes the selected slave summary.
- `PING`, `READ`, and `INFO` operate on the selected slave.

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware result: pending.

## Dashboard Mapping Rework Phase 3 - Dashboard UI Model Consumption

Status: Implemented and build-passed, pending hardware validation.

### Reference Alignment

This phase follows `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`:

- Dashboard SHALL read logical slots only.
- Dashboard SHALL support NULL slot state.
- Control widgets SHALL appear dynamically from mapped availability.

### Changed Files

- `src/ui_screens.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Main dashboard temperature average now reads `RS485State.dashboard`.
- Temperature detail page now reads mapped temperature slots and validity flags.
- CO2 widget now reads mapped CO2 value and shows NULL when invalid.
- LUX widget now reads mapped LUX value and shows NULL when invalid.
- Human presence widget now reads mapped human presence value and shows NULL when invalid.
- AC and Projector controls now appear from mapped dashboard availability.
- LCD and Light controls remain capability-based temporarily because their dedicated logical endpoints are scheduled for the next UI/config phases.

### Serial Monitor Test Commands

Use without real slave hardware:

```text
rs485 inject ok
rs485 test 0x10 0x05
rs485 stats
```

Expected behavior:

- `rs485 stats` shows assigned mapping endpoints.
- Dashboard shows NULL for mapped sensor slots until valid sensor values exist.
- AC/Projector buttons appear only when their mapped availability is true.

Use with Modbus slave hardware:

```text
rs485 poll off
rs485 sync 0x10
rs485 poll on
rs485 stats
```

Expected behavior:

- Once polling reads valid sensor registers, dashboard widgets update through `DashboardModel`.
- Offline/stale mapped slots return to NULL instead of showing old data as valid.

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware result: pending.

## Dashboard Mapping Rework Phase 2 - Mapping Engine

Status: Implemented and build-passed, pending hardware validation.

### Reference Alignment

This phase follows `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md` sections:

- Auto mapping rules.
- Dashboard runtime model.
- Offline/null slot behavior.
- Runtime flow: RS485 Manager -> Slave Registry Update -> Mapping Manager -> Dashboard Model.

### Changed Files

- `src/data.h`
- `src/mapping_manager.h`
- `src/mapping_manager.cpp`
- `src/rs485_manager.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Added per-slave cached runtime values:
  - temperature values and validity flags
  - CO2 value and validity flag
  - LUX value and validity flag
  - human presence value and validity flag
- Added `mapping_manager` module.
- Added auto-map behavior:
  - fills temperature slots from online enabled TEMP slaves
  - fills CO2, LUX, human presence, AC, and projector logical endpoints
  - does not overwrite manually assigned slots
  - keeps assigned mapping stable when a slave goes offline, but dashboard validity becomes false
- Added dashboard compose behavior:
  - updates `RS485State.dashboard`
  - invalid/unavailable slots become null-style model values
  - UI update flag is raised only when the composed dashboard model changes
- Added LUX polling:
  - `CAP_LUX` reads `0x0113` (`LUX_X10`)
- Added Serial debug visibility:
  - `rs485 stats` prints dashboard validity flags
  - `rs485 stats` prints all logical mappings

### Serial Monitor Test Commands

Use without real slave hardware:

```text
rs485 stats
rs485 inject ok
rs485 test 0x10 0x05
rs485 stats
```

Expected no-hardware behavior:

- `GET_CONFIG` seeds capability metadata.
- Mapping manager assigns logical slots for the fake available capabilities.
- `rs485 stats` shows `map[...] assigned=1` for mapped endpoints.
- Dashboard validity flags may remain `0` until sensor values are read or faked.

Use with Modbus slave hardware:

```text
rs485 poll off
rs485 sync 0x10
rs485 poll on
rs485 stats
```

Expected hardware behavior:

- Slave capability count registers are read from `0x0011..0x0016`.
- Polling reads temperature, CO2, presence, and LUX according to capability.
- Dashboard validity flags become true only for mapped, online, valid sensor data.

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware result: pending.

Missing before Phase 2:

- Confirm slave firmware register function code: holding register vs input register.
- Confirm all slave firmware variants expose the V_1_4_0 identity and capability count map.

## Phase 2 - Register Polling and Capability Registry

Status: Implemented and build-passed, pending hardware validation.

### Changed Files

- `src/data.h`
- `src/rs485_manager.h`
- `src/rs485_manager.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Added per-slave identity sync fields:
  - firmware version
  - MAC-derived UID
  - identity sync flag
  - last identity sync timestamp
- Added per-slave capability registry fields:
  - capability bitmask
  - temperature assignment/available mask
  - CO2 sensor count
  - presence sensor count
  - relay count
  - IR count
  - LCD count reserved for UI compatibility
  - capability sync flag
  - last capability sync timestamp
- Added capability constants matching `CapabilityBit` from `Smart_Building_RS485_Modbus_Architecture.md`.
- Added staged polling:
  - identity sync every 10 seconds
  - capability sync every 5 seconds
  - sensor polling only after identity/capability is current
- Added capability-based sensor polling:
  - master reads runtime register groups from the agreed slave contract
  - `CAP_TEMP` uses `0x0100..0x0103`
  - `CAP_LUX` is master/UI reserved unless a future slave contract adds LUX runtime registers
  - `CAP_CO2` uses `0x0110`
  - `CAP_PRESENCE` uses `0x0120..0x0121`
  - relay state uses `0x0130..0x0131`
- Kept polling inside `Task_RS485` on Core 0 and kept public UI/API compatibility.

### Serial Monitor Test Commands

Use without real slave hardware:

```text
rs485 stats
rs485 inject ok
rs485 test 0x10 0x02
rs485 inject ok
rs485 test 0x10 0x05
rs485 stats
```

Expected no-hardware behavior:

- Forced `GET_INFO` seeds fake identity data.
- Forced `GET_CONFIG` seeds fake capability data: TEMP + CO2 + PRESENCE.
- `rs485 stats` prints identity/capability sync flags and assignment/count fields.

Use with Modbus slave hardware:

```text
rs485 poll off
rs485 sync 0x10
rs485 stats
rs485 poll on
```

Expected hardware behavior:

- `rs485 sync 0x10` reads identity registers `0x0000..0x0008`.
- `rs485 sync 0x10` reads capability count registers `0x0011..0x0016`.
- Polling then reads sensor registers according to the slave capability bitmask.

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware result: pending.

Missing before Phase 3:

- Pairing candidate read at address `247`.
- Address assignment register definition.
- `SAVE_CONFIG` register definition (`0x00F1 = 0xA55A`).
- Dashboard mapping and control routing UI.

## Phase 3 - Pairing Candidate, Address Assignment, and UI Route

Status: Implemented and build-passed, pending hardware validation.

### Reference Alignment

This phase follows `docs/Smart_Building_RS485_Modbus_Architecture.md` sections:

- Pairing flow: DISCOVER pauses polling, slave responds on address `247`, master reads identity/capability, user assigns address, master writes new address, master writes save config, polling resumes.
- Master ownership: UI only requests pairing/assign; RS485 Manager owns Modbus and MAX3485 direction.
- Addressing: address `247` is treated as pairing/default only; assigned normal slave addresses are auto-selected from `2..246`.

### Changed Files

- `src/data.h`
- `src/data.cpp`
- `src/rs485_manager.h`
- `src/rs485_manager.cpp`
- `src/ui_screens.h`
- `src/ui_screens.cpp`
- `src/main.cpp`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Added pairing candidate state:
  - candidate ready flag
  - candidate identity/capability snapshot
  - assign request flag
  - requested assign address
- Added pairing scanner:
  - while pairing mode is active, polling is paused
  - manager periodically reads identity registers from Modbus address `247`
  - manager then reads capability registers from Modbus address `247`
  - candidate appears only after both reads pass
- Added assignment flow:
  - UI requests assignment only through callback
  - RS485 Manager writes target address to `RS485_MODBUS_REG_NODE_ADDRESS`
  - RS485 Manager writes `RS485_MODBUS_SAVE_CONFIG_VALUE` to `RS485_MODBUS_REG_SAVE_CONFIG`
  - candidate is added into configured slave list
  - pairing mode exits and polling restores
- Added Slave Manager UI route:
  - in DISCOVER mode, candidate status shows UID and capability label
  - when candidate is ready, right-side action changes to `ASSIGN AUTO`
  - tapping `ASSIGN AUTO` assigns the next free address automatically
- Added Serial debug commands:
  - `rs485 pairscan`
  - `rs485 pairfake`
  - `rs485 assign [addr]`

### Temporary Register Contract

The architecture document defines the pairing concept but does not yet define the Modbus register addresses for address assignment and save-config.

Phase 3 uses these temporary master/slave contract constants:

| Register | Purpose |
|---|---|
| `0x00F0` | `NODE_ADDRESS` write target new Modbus address |
| `0x00F1` | `SAVE_CONFIG` write `0xA55A` to persist config |

Slave firmware must implement the same temporary registers for hardware pairing to work.

### Serial Monitor Test Commands

Use without real slave hardware:

```text
rs485 pairfake
rs485 stats
rs485 assign 0
rs485 stats
```

Expected no-hardware behavior:

- `rs485 pairfake` creates an in-memory pairing candidate.
- `rs485 assign 0` assigns the next free address automatically.
- Because `assign` performs actual Modbus writes to address `247`, it will fail without hardware unless the slave exists. This is intentional; it prevents fake assignment from hiding a missing slave.

Use with Modbus slave hardware:

```text
rs485 poll off
rs485 pair 30000
rs485 pairscan
rs485 stats
rs485 assign 0
rs485 poll on
rs485 stats
```

Expected hardware behavior:

- Slave in pairing mode responds on address `247`.
- Candidate shows UID/capability in UI and Serial stats.
- `assign 0` writes a free address, saves config, registers the slave, exits pairing, and restores polling.

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware result: pending.

Known Remaining Work

- Hardware validation with slave firmware implementing `0x00F0` and `0x00F1`.
- Persistent slave registry storage across reboot.
- Dashboard logical mapping UI and control routing detail screens.

## Dashboard Mapping Rework Phase 1 - Data Contract Alignment

Status: Implemented and build-passed, pending hardware validation.

### Reference Alignment

This phase follows `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`:

- Master owns slave registry and dashboard mapping.
- Dashboard will read logical slots instead of direct slave/register values.
- LUX is now a first-class capability.
- Human presence remains a semantic capability, not tied to PIR naming.

### Changed Files

- `src/data.h`
- `src/data.cpp`
- `src/rs485_manager.h`
- `src/rs485_manager.cpp`
- `src/ui_screens.cpp`
- `docs/RS485_Modbus_Slave_Firmware_Contract.md`
- `docs/Smart_Building_RS485_Modbus_Architecture.md`
- `docs/FSD_Smart_Building_Master_UPDATED.md`
- `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`
- `docs/RS485_agent_documentation_report.md`

### What Was Added

- Added shared capability constants in master state:
  - `CAP_TEMP`
  - `CAP_CO2`
  - `CAP_HUMAN_PRESENCE`
  - `CAP_AC_IR`
  - `CAP_PROJECTOR_IR`
  - `CAP_LIGHT_RELAY`
  - `CAP_LUX`
  - `CAP_LCD_CTRL`
- Resolved the LUX/LCD bit conflict:
  - `CAP_LUX = 1 << 6`
  - `CAP_LCD_CTRL = 1 << 7`
- Updated capability register block:
  - `0x0016 = LUX_SENSOR_COUNT`
  - `0x0016 = LCD_CTRL_COUNT`
  - master now reads capability count block `0x0011..0x0016`.
- Added registry-ready fields to `RS485SlaveState`:
  - `mac`
  - `name`
  - `enabled_mask`
  - `lux_count`
- Added logical dashboard foundations:
  - `DashboardLogicalId`
  - `LogicalMapping`
  - `DashboardModel`
- Initialized default mappings and dashboard validity flags in `data.cpp`.
- Updated Serial stats to print MAC, enabled mask, LUX count, and LCD count.
- Updated Slave Manager capability labels to show `LUX`.

### Serial Monitor Test Commands

Use without real slave hardware:

```text
rs485 stats
rs485 inject ok
rs485 test 0x10 0x05
rs485 stats
rs485 pairfake
rs485 stats
```

Expected behavior:

- Forced `GET_CONFIG` now seeds TEMP + CO2 + HUMAN/PRESENCE + LUX.
- `rs485 stats` prints `LUX` count and `enabled=0x....`.
- `rs485 pairfake` creates a candidate with LUX capability.

### Phase Gate Result

Build result: PASS (`pio run`, ESP32-S3, DFRobot_RTU 1.0.6 resolved).

Hardware result: pending.
