
# Smart Building RS485 Modbus Communication Architecture
## Capability-Driven Master-Orchestrated Design

## Project
Smart Building Master S3 + Distributed Slave Network

## Document Purpose

Dokumen ini mendefinisikan arsitektur komunikasi RS485 terbaru menggunakan:

- Modbus RTU via DFRobot_RTU Library
- Slave wire contract v2.0.0: `docs/RS485_Modbus_Slave_Firmware_Contract_V_2.0.0.md`

Dokumen ini mencakup:
- RS485 transport architecture
- Modbus register philosophy
- Capability-driven slave model
- Master-orchestrated runtime behavior
- Discovery & pairing flow
- Slave identity system
- Runtime role assignment
- Single-sensor & multi-sensor compatibility
- Dashboard integration model
- Control routing
- Reliability philosophy
- Slave implementation guideline

Tujuan utama:

Membuat sistem slave fleksibel, scalable, mudah diremix, dan tetap kompatibel dengan:
- 1 slave 1 sensor
- multi-sensor slave

---

# 1. Core System Philosophy

Slave SHALL NOT menentukan dirinya menjadi apa.

Slave hanya menyediakan:
- capability
- sensor
- channel
- control endpoint

Master menentukan:
- role
- mapping
- dashboard slot
- control purpose
- runtime behavior
- active feature

---

# 2. Architecture Philosophy

Slave = Dumb Capability Node

Master = System Brain / Orchestrator

Slave tidak memiliki knowledge terhadap:
- dashboard layout
- room logic
- temp slot order
- IR target purpose
- UI behavior

Master memiliki seluruh orchestration logic.

---

# 3. Supported Slave Model

Mode A:
1 Slave = 1 Sensor Type

Contoh:
Slave A:
TEMP only

Mode B:
1 Slave = Multiple Capability

Contoh:
Slave B:
TEMP + CO2 + PIR + IR

---

# 4. Communication Stack

The implementation has two related flows: runtime data flow and UI command flow.

Runtime data flow:

```text
RS485 BUS
    ->
UART + MAX3485
    ->
DFRobot_RTU Library
    ->
RS485 Manager
    ->
Slave Registry
    ->
Mapping Manager
    ->
Dashboard Model
    ->
Dashboard/UI
```

UI command/request flow:

```text
Dashboard/UI
    ->
UI callbacks / state request flags
    ->
RS485 Manager
    ->
DFRobot_RTU Library
    ->
UART + MAX3485
    ->
RS485 BUS
```

Dashboard/UI SHALL NOT access UART, MAX3485 direction control, Modbus parser, or raw register transport directly.

Implementation note:

```text
RS485State.slaves[]      = Slave Registry
RS485State.mappings[]    = Logical Mapping table
RS485State.dashboard     = Dashboard Model
mapping_manager_update_locked()
    = recompute logical mappings and compose Dashboard Model
```

The code still exposes compatibility/debug types such as `RS485Frame`, `RS485Command`, sequence counters, and transaction result wrappers. These are firmware API/debug compatibility layers only. They SHALL NOT be interpreted as the active wire protocol. The active RS485 wire transport is Modbus RTU through DFRobot_RTU.

Physical transport stack:

```text
RS485 Manager
    ->
DFRobot_RTU Library
    ->
UART + MAX3485
    ->
RS485 BUS
```

---

# 5. Modbus Philosophy

System menggunakan Modbus RTU untuk:
- framing
- CRC
- ACK behavior
- timeout handling
- request/response

ACK dianggap valid ketika slave memberikan Modbus response valid.

---

# 6. Bus Ownership Rule

Master owns the bus.

Slave SHALL NOT transmit without request.

Slave hanya boleh respond ketika:
- polling
- read request
- write request
- pairing mode request

---

# 7. Addressing Scheme

Recommended:

1 = Reserved
2-246 = Normal assigned slave
247 = Pairing/default address
248-254 = Reserved/testing

Per slave contract v2.0.0, all slaves boot at address 247 because slave config is RAM-only. The master owns persistent MAC to assigned-address mapping and restores known slaves after reboot.

---

# 8. Pairing Flow

1. User taps DISCOVER
2. Master pauses polling
3. Pairing countdown starts
4. Master scans pairing/default address 247
5. Slave is already listening on address 247 after boot
6. Master reads identity registers
7. Master reads capability assignment/count registers
8. UI shows MAC/identity + capability assignment/counts
9. User assigns address
10. Master writes capability assignment/count registers to 0x0010-0x0017
11. Master writes new address to NODE_ADDRESS 0x0000
12. Slave applies address immediately and leaves address 247
13. Master stores MAC -> assigned address mapping
14. Polling resumes on the assigned address

SAVE_CONFIG at 0x00F0 remains an optional compatibility/config signal. Slave v2.0.0 SHALL NOT rely on local EEPROM persistence for address or capability; master persistence is authoritative.

Recovery flow after slave reboot:

1. Slave reboots and returns to address 247
2. Master reads identity/MAC at address 247
3. Master looks up saved MAC -> assigned address
4. Master writes recovery MAC to 0x00F5-0x00F7
5. Master writes recovered address to 0x00F8
6. Matching slave applies the recovered address
7. Non-matching slaves ignore the recovery write and remain at 247

---

# 9. Slave Identity Registers

0x0000 NODE_ADDRESS
0x0001 FW_VERSION
0x0002 MAC_0_1
0x0003 MAC_2_3
0x0004 MAC_4_5

MAC registers are the stable identity source used by pairing and recovery. Contract v2.0.0 removes DEVICE_MAGIC, protocol, device-class, and UID identity registers from the active wire map. Master firmware may derive an internal UID from MAC for registry persistence.

---

# 10. Capability Registers

0x0010 TEMP_SENSOR_ASSIGNMENT       4-bit mask, bit3..bit0 = Temp1..Temp4
0x0011 LUX_SENSOR_ASSIGNMENT        4-bit mask, bit3..bit0 = Lux1..Lux4
0x0012 CO2_SENSOR_COUNT
0x0013 PRESENCE_SENSOR_ASSIGNMENT   4-bit mask, bit3..bit0 = Presence1..Presence4
0x0014 RELAY_ASSIGNMENT             2-bit mask, bit1..bit0 = Relay1..Relay2
0x0015 IR_PROJECTOR_ENABLE
0x0016 IR_AC_1_ENABLE
0x0017 IR_AC_2_ENABLE

The active wire contract uses these assignment/count registers. In the current master UI, the Slave Detail checklist is the owner of assignment state. Reading these registers from a slave is useful for sync/diagnostics, but it SHALL NOT override the master's checked/unchecked state after the user has configured it.

When the user presses SAVE in Slave Detail, the master writes the current assignment state to `0x0010..0x0017`. For temperature:

```text
Temp 1 -> 0x0010 bit3 / 0x0008
Temp 2 -> 0x0010 bit2 / 0x0004
Temp 3 -> 0x0010 bit1 / 0x0002
Temp 4 -> 0x0010 bit0 / 0x0001
```

The master then may write `0x00F0 = 0xA55A` as a compatibility SAVE_CONFIG signal.

---

# 11. Capability Bitmask

This bitmask is an internal master/UI convenience model, not the v2.0.0 wire contract.

enum CapabilityBit {
    CAP_TEMP
    CAP_CO2
    CAP_PRESENCE
    CAP_AC_IR
    CAP_PROJECTOR_IR
    CAP_LIGHT_RELAY
    CAP_LUX
    CAP_LCD_CTRL     // reserved UI capability, not present in v2.0.0 slave wire map
}

---

# 12. Single Sensor Slave Example

Slave A assigned by master:

```text
0x0010 TEMP_SENSOR_ASSIGNMENT = 0x0008
```

Runtime register:

```text
0x0100 TEMP_1_X10
```

---

# 13. Multi Sensor Slave Example

Slave B assigned by master:

```text
0x0010 TEMP_SENSOR_ASSIGNMENT     = 0x000C  // Temp 1 + Temp 2
0x0012 CO2_SENSOR_COUNT           = 1
0x0013 PRESENCE_SENSOR_ASSIGNMENT = 0x0008  // Presence 1
```

Runtime registers:

```text
0x0100 TEMP_1_X10
0x0101 TEMP_2_X10
0x0108 CO2_PPM
0x0109 PRESENCE_1_STATE
```

---

# 14. Runtime Role Assignment

Slave SHALL NOT permanently know:
- dashboard slot
- room role
- AC purpose
- projector purpose

Master assigns runtime behavior dynamically.

---

# 15. Dashboard Philosophy

Dashboard membaca logical slot.

Dashboard tidak membaca slave secara langsung.

---

# 16. Temperature Dashboard Behavior

Home dashboard hanya menampilkan:

AVG TEMPERATURE

AVG dihitung dari seluruh temp slot valid.

Jika user tap AVG:
show detailed 4-point temperature page.

---

# 17. Dynamic Control Visibility

IF AC capability detected:
show AC control

IF projector capability detected:
show projector control

IF LCD capability detected:
show LCD control

Jika tidak tersedia:
hide widget completely

Current firmware alignment:

```text
Implemented through DashboardModel:
- Temperature slots
- CO2
- Lux
- Human Presence
- AC availability
- Projector availability

Implemented as direct registry-derived dashboard visibility:
- Light Relay / LED

Capability visible in Slave Detail / feature checklist, but not yet a main-dashboard logical control:
- LCD Control
```

Until `LOGICAL_LIGHT_RELAY_CONTROL` and/or `LOGICAL_LCD_CONTROL` are added to `DashboardLogicalId`, Light Relay and LCD SHALL be documented as partially integrated capabilities.

---

# 18. Logical Mapping Example

TEMP_SLOT_1 -> Slave A TEMP_1
TEMP_SLOT_2 -> Slave A TEMP_2
TEMP_SLOT_3 -> Slave B TEMP_1
TEMP_SLOT_4 -> Slave B TEMP_2

Dashboard hanya membaca slot.

Temperature slot numbering is fixed:

```text
TEMP_SLOT_1 / Point 1 -> Temp 1 -> 0x0100
TEMP_SLOT_2 / Point 2 -> Temp 2 -> 0x0101
TEMP_SLOT_3 / Point 3 -> Temp 3 -> 0x0102
TEMP_SLOT_4 / Point 4 -> Temp 4 -> 0x0103
```

The same temperature channel number SHALL NOT be enabled on multiple slaves at the same time from the Slave Detail UI. If Temp 1 is checked on one slave, Temp 1 appears Unavailable on other slaves until it is unchecked.

Current firmware logical slots:

```text
TEMP_SLOT_1
TEMP_SLOT_2
TEMP_SLOT_3
TEMP_SLOT_4
CO2_MAIN
LUX_MAIN
HUMAN_PRESENCE_MAIN
AC_CONTROL
PROJECTOR_CONTROL
```

Current firmware does not yet define dedicated logical dashboard slots for:

```text
LIGHT_RELAY_CONTROL
LCD_CONTROL
```

Therefore, LED/Light Relay dashboard visibility currently comes from enabled online slave capability, not from a persisted logical mapping slot.

---

# 19. NULL / Offline Behavior

Jika slot belum memiliki source:
display "--"

Jika slave offline:
mapping tetap ada
tetapi value menjadi invalid/stale

Jika slave online kembali:
mapping restored automatically

---

# 20. Sensor Register Layout

0x0100 TEMP_1_X10
0x0101 TEMP_2_X10
0x0102 TEMP_3_X10
0x0103 TEMP_4_X10

Light level:
0x0104 LUX_1_LX
0x0105 LUX_2_LX
0x0106 LUX_3_LX
0x0107 LUX_4_LX

Air Quality:
0x0108 CO2_PPM

Presence:
0x0109 PRESENCE_1_STATE
0x010A PRESENCE_2_STATE
0x010B PRESENCE_3_STATE
0x010C PRESENCE_4_STATE

Relay:
0x010D RELAY_1_STATE
0x010E RELAY_2_STATE

The master reads this runtime block as one contiguous `0x0100` length-15 Modbus read. Invalid/unassigned values SHALL follow the v2.0.0 sentinel values from the slave contract.

---

# 21. Control Register Layout

AC:
0x0200 AC1_POWER
0x0201 AC1_SET_TEMP
0x0202 AC1_MODE
0x0203 AC2_POWER
0x0204 AC2_SET_TEMP
0x0205 AC2_MODE

Projector:
0x0210 PROJECTOR_POWER
0x0211 PROJECTOR_INPUT

Current firmware control implementation status:

```text
Implemented active write path:
- RS485_CMD_SET_OUTPUT -> write single holding register 0x010D RELAY_1_STATE

Target / not yet fully implemented as active command paths:
- 0x0200 AC1_POWER
- 0x0201 AC1_SET_TEMP
- 0x0202 AC1_MODE
- 0x0203 AC2_POWER
- 0x0204 AC2_SET_TEMP
- 0x0205 AC2_MODE
- 0x0210 PROJECTOR_POWER
- 0x0211 PROJECTOR_INPUT
```

Until dedicated AC/Projector command routing is implemented, these `0x0200+` registers SHALL be treated as target contract registers, not as already-wired master command paths.

---

# 22. Reliability Philosophy

Master tetap wajib maintain:
- online state
- degraded state
- timeout counter
- exception counter
- retry policy

---

# 23. Polling Philosophy

Recommended:
- Sensor polling: 500-1000 ms
- Identity polling: 5-10 s
- Config sync: on demand

Current firmware values:

```text
Sensor polling interval:      1000 ms
Identity sync interval:       10000 ms
Capability sync interval:      5000 ms
Pairing scan interval:          700 ms
Modbus response timeout:        100 ms
Retry count:                      1
Offline timeout:              5000 ms
Degraded fail threshold:          3 consecutive failures
Offline fail threshold:           5 consecutive failures
```

---

# 24. Slave Design Guideline

Slave SHOULD:
- expose raw sensor data
- expose assignment/count registers required by the v2.0.0 contract
- respond to Modbus request
- avoid orchestration logic

Slave SHOULD NOT:
- decide dashboard slot
- decide room assignment
- decide AC purpose
- decide projector purpose

---

# 25. Master Design Guideline

Master SHALL own:
- orchestration
- mapping
- dashboard logic
- role assignment
- visibility logic
- AVG calculation
- control routing

---

# 26. Final Engineering Principles

1. Dashboard SHALL NOT hardcode slave address.
2. Dashboard SHALL use logical slot only.
3. Slave SHALL expose capability, not final role.
4. Master SHALL assign runtime purpose.
5. Single-sensor slave SHALL be supported.
6. Multi-sensor slave SHALL be supported.
7. Dynamic dashboard widget visibility SHALL be supported.
8. AVG temperature SHALL be primary dashboard temperature.
9. Detailed temperature SHALL exist on detail page.
10. Modbus RTU SHALL be primary RS485 transport.
11. UI command requests SHALL go through RS485 Manager callbacks/state flags.
12. Runtime sensor data SHALL flow from RS485 Manager into Slave Registry, then Mapping Manager, then Dashboard Model.
13. Any capability shown on the main dashboard SHOULD eventually be represented by a logical mapping slot. Current exception: Light Relay / LED is still registry-derived.
14. Compatibility structs/commands in firmware SHALL be treated as API/debug wrappers around Modbus operations, not a separate active wire protocol.

---

# END DOCUMENT
