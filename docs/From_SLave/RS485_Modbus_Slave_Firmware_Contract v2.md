# RS485 Modbus Slave Firmware Contract

Version: v2.2.0-draft

## Purpose

Dokumen ini adalah kontrak implementasi firmware slave untuk Smart Building RS485 Modbus.

Tujuan v2.2.0-draft:

- Merapikan inkonsistensi register v2.0.1.
- Menetapkan recovery mechanism final.
- Menetapkan ownership persistent storage.
- Menetapkan capability policy final.
- Menambahkan production-oriented device profile model.
- Menyesuaikan desain dengan implementasi Smart Building Binus deployment.
- Menambahkan register AC fan speed dan swing/vane untuk IR AC control.

Master firmware memakai:

- Modbus RTU.
- `DFRobot_RTU` sebagai Modbus master library.
- UART0 board signal `TXD0/RXD0`.
- MAX3485 half-duplex direction pin di master: `COM_SW = GPIO21`.
- Baudrate `19200`.
- Format serial `8N1`.

Dokumen ini dipakai supaya firmware slave punya register map, default config, error convention, dan behavior yang sama dengan master.

## Core Principles

- Master is the single source of truth.
- Slave is RAM-only.
- Slave exposes registers and executes Modbus requests.
- Slave does not own room, dashboard, naming, building, profile, or capability policy.
- Master owns persistent device registry.
- Auto recovery for known devices is allowed and required.
- Auto pairing for unknown devices is not allowed.

## Persistent Storage Ownership

### Master

Master SHALL store persistent device registry.

Persistent registry minimum fields:

- MAC address.
- Assigned address.
- Device profile.
- Device name.
- Room name.

Runtime registry fields:

- Last seen timestamp.
- Online status.

`Last seen timestamp` and `Online status` SHOULD be treated as runtime state. They SHOULD NOT be written to persistent storage on every poll, to avoid flash wear. If persistence is needed, write them only on important events or at a low-frequency interval.

Master persistent storage may use EEPROM, NVS, Preferences, filesystem, or another storage abstraction. The contract requirement is persistence behavior, not a specific storage backend.

Master SHALL provide:

- Device rename.
- Device delete.
- Manual recovery.
- Re-pair.

### Slave

Slave SHALL remain RAM-only.

Slave SHALL NOT use these for address or capability persistence:

- EEPROM.
- Preferences.
- NVS.
- Filesystem.

After reboot:

- Address returns to `247`.
- Pairing/recovery waiting state resumes.

## Electrical / UART Config

Slave UART:

```cpp
#define SLAVE_RS485_BAUDRATE 19200
#define SLAVE_RS485_SERIAL_CONFIG SERIAL_8N1
```

Transceiver:

- MAX3485 / compatible 3.3V RS485 transceiver.
- DE and `/RE` should be controlled together by one direction pin if available.
- Direction LOW = receive.
- Direction HIGH = transmit.

Master PCB note:

- Master uses `COM_SW` on GPIO21 to drive MAX3485 `DE + /RE`.

Slave PCB note for Wemos Lolin C3 Mini test:

- RX = GPIO 20.
- TX = GPIO 21.
- COM_SW / RS485_DIR = GPIO 2.

Address constants:

```cpp
#define MODBUS_ADDR_RESERVED_MASTER 1
#define MODBUS_ADDR_MIN_NORMAL      2
#define MODBUS_ADDR_MAX_NORMAL      246
#define MODBUS_ADDR_PAIRING         247
```

Rules:

- Address `247` is reserved for default pairing/recovery waiting mode.
- Normal assigned address must be `2..246`.
- Address `1` is reserved by system convention.
- Master SHALL guarantee assigned address uniqueness.
- Slave SHALL NOT perform address conflict detection because it has no visibility of other devices on the RS485 bus.

## Required Modbus Function Codes

Slave SHALL support responding to:

| Function | Name                            | Required |
| -------- | ------------------------------- | -------- |
| `0x01`   | Read Coils                      | Yes      |
| `0x03`   | Read Holding Registers          | Yes      |
| `0x04`   | Read Input Registers            | Yes      |
| `0x06`   | Write Single Holding Register   | Yes      |
| `0x10`   | Write Multiple Holding Register | Yes      |

## Register Endianness

All registers are 16-bit unsigned Modbus registers.

Multi-register fields use big logical order:

```text
MAC bytes:
  MAC_0_1 high byte = MAC[0]
  MAC_0_1 low byte  = MAC[1]
```

Signed sensor values use signed 16-bit interpretation on the register value.

Example:

```cpp
int16_t temp_x10 = (int16_t)holding_register[0x0100];
```

## Register Map

Only this register map is valid for v2.2.0-draft. Tables, header definitions, recovery documentation, and changelog SHALL use the same numbering.

### Identity Registers

Base: `0x0000`

| Register | Name           | Type       | Access | Required | Description                   |
| -------- | -------------- | ---------- | ------ | -------- | ----------------------------- |
| `0x0000` | `NODE_ADDRESS` | `uint16_t` | R/W    | Yes      | Current or new Modbus address |
| `0x0001` | `FW_VERSION`   | `uint16_t` | R      | Yes      | Example `210` for v2.1.0      |
| `0x0002` | `MAC_0_1`      | `uint16_t` | R      | Yes      | MAC bytes 0 and 1             |
| `0x0003` | `MAC_2_3`      | `uint16_t` | R      | Yes      | MAC bytes 2 and 3             |
| `0x0004` | `MAC_4_5`      | `uint16_t` | R      | Yes      | MAC bytes 4 and 5             |

### Capability Registers

Base: `0x0010`

These registers are written by the master during pairing or reconfiguration. A value of `0` means the capability is disabled.

Slave SHALL store these values in RAM only.

Slave SHALL NOT enforce device profile policy. The master owns policy and decides which registers are valid for a given device profile.

| Register | Name                         | Access | Required | Description                                                           |
| -------- | ---------------------------- | ------ | -------- | --------------------------------------------------------------------- |
| `0x0010` | `TEMP_SENSOR_ASSIGNMENT`     | R/W    | Yes      | Bitmask assignment for Temp 1-4. Read from MSB of a 4-bit nibble.     |
| `0x0011` | `LUX_SENSOR_ASSIGNMENT`      | R/W    | Yes      | Bitmask assignment for Lux 1-4. Read from MSB of a 4-bit nibble.      |
| `0x0012` | `CO2_SENSOR_COUNT`           | R/W    | Yes      | How many CO2 sensors.                                                 |
| `0x0013` | `PRESENCE_SENSOR_ASSIGNMENT` | R/W    | Yes      | Bitmask assignment for Presence 1-4. Read from MSB of a 4-bit nibble. |
| `0x0014` | `RELAY_ASSIGNMENT`           | R/W    | Yes      | Bitmask assignment for Relay 1-2. Read from MSB of a 2-bit nibble.    |
| `0x0015` | `IR_PROJECTOR_ENABLE`        | R/W    | Yes      | `0` disable, `1` enable IR for Projector.                             |
| `0x0016` | `IR_AC_1_ENABLE`             | R/W    | Yes      | `0` disable, `1` enable IR for AC 1.                                  |
| `0x0017` | `IR_AC_2_ENABLE`             | R/W    | Yes      | `0` disable, `1` enable IR for AC 2.                                  |

Temperature sensor assignment:

- Bit `3` value `8`: Temp 1.
- Bit `2` value `4`: Temp 2.
- Bit `1` value `2`: Temp 3.
- Bit `0` value `1`: Temp 4.
- `0x0000` disables temperature reading.

Lux sensor assignment:

- Bit `3` value `8`: Lux 1.
- Bit `2` value `4`: Lux 2.
- Bit `1` value `2`: Lux 3.
- Bit `0` value `1`: Lux 4.
- `0x0000` disables lux reading.

Presence sensor assignment:

- Bit `3` value `8`: Presence 1.
- Bit `2` value `4`: Presence 2.
- Bit `1` value `2`: Presence 3.
- Bit `0` value `1`: Presence 4.
- `0x0000` disables presence reading.

Relay assignment:

- Bit `1` value `2`: Relay 1.
- Bit `0` value `1`: Relay 2.
- `0x0000` disables relay control.

IR enable:

- `IR_PROJECTOR_ENABLE`: `0` disable, `1` enable IR for Projector.
- `IR_AC_1_ENABLE`: `0` disable, `1` enable IR for AC 1.
- `IR_AC_2_ENABLE`: `0` disable, `1` enable IR for AC 2.

### Config / Recovery Registers

Base: `0x00F0`

| Register | Name                    | Access | Required    | Description                               |
| -------- | ----------------------- | ------ | ----------- | ----------------------------------------- |
| `0x00F0` | `CONFIG_VERSION`        | R      | Recommended | Increment if config layout changes        |
| `0x00F1` | `LAST_ERROR`            | R/W    | Recommended | Last slave error code, write `0` to clear |
| `0x00F2` | `UPTIME_LOW`            | R      | Optional    | Uptime lower 16-bit seconds               |
| `0x00F3` | `UPTIME_HIGH`           | R      | Optional    | Uptime upper 16-bit seconds               |
| `0x00F4` | `RECOVERY_MAC_0_1`      | W      | Yes         | MAC bytes 0 and 1 for recovery matching   |
| `0x00F5` | `RECOVERY_MAC_2_3`      | W      | Yes         | MAC bytes 2 and 3 for recovery matching   |
| `0x00F6` | `RECOVERY_MAC_4_5`      | W      | Yes         | MAC bytes 4 and 5 for recovery matching   |
| `0x00F7` | `RECOVERY_NODE_ADDRESS` | W      | Yes         | Address to apply when recovery MAC matches |

### Sensor Registers

Master read recommendation:

- Read all sensor/actuator state registers in one transaction: `0x0100` length `15`.
- This covers `0x0100` to `0x010E`.
- Master should still apply profile, assignment masks, and invalid/unassigned sentinel rules when parsing each value.

| Register | Name               | Type       | Unit                              |
| -------- | ------------------ | ---------- | --------------------------------- |
| `0x0100` | `TEMP_1_X10`       | `int16_t`  | Celsius x10                       |
| `0x0101` | `TEMP_2_X10`       | `int16_t`  | Celsius x10                       |
| `0x0102` | `TEMP_3_X10`       | `int16_t`  | Celsius x10                       |
| `0x0103` | `TEMP_4_X10`       | `int16_t`  | Celsius x10                       |
| `0x0104` | `LUX_1_LX`         | `uint16_t` | lux                               |
| `0x0105` | `LUX_2_LX`         | `uint16_t` | lux                               |
| `0x0106` | `LUX_3_LX`         | `uint16_t` | lux                               |
| `0x0107` | `LUX_4_LX`         | `uint16_t` | lux                               |
| `0x0108` | `CO2_PPM`          | `uint16_t` | ppm                               |
| `0x0109` | `PRESENCE_1_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010A` | `PRESENCE_2_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010B` | `PRESENCE_3_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010C` | `PRESENCE_4_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010D` | `RELAY_1_STATE`    | `uint16_t` | `0` off, `1` on                   |
| `0x010E` | `RELAY_2_STATE`    | `uint16_t` | `0` off, `1` on                   |

Sensor invalid values and assignment rules:

| Condition            | Modbus Type         | Value               |
| -------------------- | ------------------- | ------------------- |
| Sensor reading error | Signed `int16_t`    | `-32768` (`0x8000`) |
| Sensor not assigned  | Signed `int16_t`    | `-32767` (`0x8001`) |
| Sensor reading error | Unsigned `uint16_t` | `0xFFFF` (`65535`)  |
| Sensor not assigned  | Unsigned `uint16_t` | `0xFFFE` (`65534`)  |

If a value matches "sensor not assigned", master should ignore it. If a value matches "reading error", master can flag the sensor as offline or degraded.

### Control Registers

AC 1:

| Register | Name                    | Access | Values                                         |
| -------- | ----------------------- | ------ | ---------------------------------------------- |
| `0x0200` | `AC_1_POWER`            | R/W    | `0` off, `1` on                                |
| `0x0201` | `AC_1_SET_TEMP`         | R/W    | Celsius x10, `160..300`, step `10`             |
| `0x0202` | `AC_1_MODE`             | R/W    | `0` cool, `1` dry, `2` fan, `3` heat, `4` auto |
| `0x0206` | `AC_1_COMMAND_STATUS`   | R      | `0` idle, `1` success, `2` busy, `3` failed    |
| `0x0208` | `AC_1_FAN_SPEED`        | R/W    | Fan speed enum below                           |
| `0x0209` | `AC_1_SWING_VERTICAL`   | R/W    | Swing/vertical vane enum below                 |
| `0x020A` | `AC_1_SWING_HORIZONTAL` | R/W    | Optional horizontal vane enum below            |

AC 2:

| Register | Name                    | Access | Values                                         |
| -------- | ----------------------- | ------ | ---------------------------------------------- |
| `0x0203` | `AC_2_POWER`            | R/W    | `0` off, `1` on                                |
| `0x0204` | `AC_2_SET_TEMP`         | R/W    | Celsius x10, `160..300`, step `10`             |
| `0x0205` | `AC_2_MODE`             | R/W    | `0` cool, `1` dry, `2` fan, `3` heat, `4` auto |
| `0x0207` | `AC_2_COMMAND_STATUS`   | R      | `0` idle, `1` success, `2` busy, `3` failed    |
| `0x020B` | `AC_2_FAN_SPEED`        | R/W    | Fan speed enum below                           |
| `0x020C` | `AC_2_SWING_VERTICAL`   | R/W    | Swing/vertical vane enum below                 |
| `0x020D` | `AC_2_SWING_HORIZONTAL` | R/W    | Optional horizontal vane enum below            |

AC fan speed enum:

| Value | Meaning |
| ----- | ------- |
| `0`   | Auto |
| `1`   | Low |
| `2`   | Medium |
| `3`   | High |
| `4`   | Quiet/Silent |
| `5`   | Turbo/Powerful |
| `6..98` | Reserved |
| `99`  | No change / unsupported |

AC swing/vertical vane enum:

| Value | Meaning |
| ----- | ------- |
| `0`   | Off / fixed |
| `1`   | Auto swing |
| `2`   | Up |
| `3`   | Mid-up |
| `4`   | Middle |
| `5`   | Mid-down |
| `6`   | Down |
| `7`   | Step next |
| `8`   | Step previous |
| `9`   | Auto comfort |
| `10`  | Auto powerful |
| `11..98` | Reserved |
| `99`  | No change / unsupported |

AC horizontal vane enum:

- Uses the same values as vertical vane when the AC IR protocol supports
  horizontal vane control.
- If the AC/protocol does not support horizontal vane control, slave SHALL
  accept `0` or `99` and SHALL ignore other values safely.

Projector:

| Register | Name                       | Access | Values                                      |
| -------- | -------------------------- | ------ | ------------------------------------------- |
| `0x0210` | `PROJECTOR_POWER`          | R/W    | `0` off, `1` on                             |
| `0x0211` | `PROJECTOR_INPUT`          | R/W    | Slave-defined enum                          |
| `0x0212` | `PROJECTOR_COMMAND_STATUS` | R      | `0` idle, `1` success, `2` busy, `3` failed |

Command status indicates command execution result only. It does not indicate actual AC/projector state because IR communication is one-way.

## Device Profile Model

Previous idea:

- One main capability plus optional Lux.

v2.1.0 replacement:

- Device profile enforcement.

Policy SHALL be enforced by master.

Slave SHALL remain policy-blind.

Device profile is stored in master persistent registry only. Slave does not need a device profile register.

### TEMP_NODE

Allowed:

- Temperature sensors.
- Optional Lux.

### PRESENCE_NODE

Allowed:

- Presence sensors.
- Optional Lux.

### CO2_NODE

Allowed:

- CO2 sensors.
- Optional Lux.

### RELAY_NODE

Allowed:

- Relay outputs.
- Optional Lux.

### IR_COMBO_NODE

Allowed:

- IR AC 1.
- IR AC 2.
- IR Projector.
- Optional Lux.

Reason:

- Reduce hardware cost.
- Reduce PCB count.
- Reduce installation complexity.
- Reduce maintenance cost.

This exception is intentional and production-driven.

## Pairing Behavior

Unassigned slave SHALL remain on address `247` indefinitely.

No timeout is required.

Pairing flow:

```text
BOOT
  active_address = 247
  listening_address = 247
  expose identity registers on address 247
  wait for recovery or pairing command

PAIRING_RX_CAPABILITY
  master writes capability assignment/count registers
  slave stores values in RAM
  slave remains policy-blind
  still listening on address 247

PAIRING_RX_ADDRESS
  master writes 0x0000 with new address at address 247
  slave validates new_address in range 2..246
  active_address = new_address
  listening_address = new_address
  exit pairing waiting mode

NORMAL_OPERATION
  listen only on assigned address until reboot
```

## Recovery Mechanism

Recovery is automatic for known devices.

Auto recovery SHALL remain available after:

- Master boot.
- Slave restart.
- Communication loss.

Recovery flow:

```text
Master loads persistent registry
Master detects a known device offline
Master writes recovery MAC + recovery address to address 247
Every slave currently at address 247 receives the request
Each slave compares all 6 MAC bytes
Matching slave applies recovery address and exits address 247
Non-matching slaves ignore the request and remain at address 247
```

Recovery write transaction:

```text
Write 247:0x00F4 length 4
  0x00F4 = RECOVERY_MAC_0_1
  0x00F5 = RECOVERY_MAC_2_3
  0x00F6 = RECOVERY_MAC_4_5
  0x00F7 = RECOVERY_NODE_ADDRESS
```

Recovery validation:

- Slave SHALL compare all 6 MAC bytes.
- Recovery SHALL only be accepted if full MAC matches.
- If MAC does not match, slave SHALL ignore the request and remain at `247`.
- Slave SHALL validate recovery address is in range `2..246` before applying it.

Response collision note:

- Multiple slaves may currently be listening on address `247`.
- Multiple non-matching slaves may answer the same write transaction.
- Master SHALL ignore Modbus response collision/error for the recovery write transaction only.
- Master SHALL confirm recovery by polling the recovered assigned address after the write.

## Discovery / Unknown Device Behavior

Auto recovery is allowed.

Auto pairing unknown devices is not allowed.

If a slave appears at address `247` and its MAC is not in master registry:

- Master SHALL NOT automatically pair the device.
- Master SHALL NOT automatically assign address.
- Master SHALL NOT automatically assign capability.
- Master SHALL NOT automatically add the device into registry.

Unknown device state:

```text
UNPAIRED_DEVICE_DETECTED
```

Unknown device waits for user action:

- Discover / Pair Device.

Summary:

Allowed:

- Auto recovery for known device.
- Auto reconnect after power loss.
- Auto restore address from registry.

Not allowed:

- Auto pairing unknown device.
- Auto assigning capability to unknown device.
- Auto adding unknown device into registry.

Recovery is automatic. Discovery is user initiated.

## Device Registry UI Requirement

Master software and FSD SHALL include Saved Device Registry.

Fields:

- Device name.
- Room.
- MAC.
- Address.
- Device profile.
- Status.
- Last seen.

Actions:

- Rename.
- Delete.
- Recover.
- Re-pair.

## Error Registers and Codes

Slave error register:

```cpp
#define REG_LAST_ERROR 0x00F1
```

Error code values:

| Code | Name                          | Meaning                        |
| ---- | ----------------------------- | ------------------------------ |
| `0`  | `SLAVE_ERR_NONE`              | No error                       |
| `1`  | `SLAVE_ERR_SENSOR_TIMEOUT`    | Sensor read timeout            |
| `2`  | `SLAVE_ERR_SENSOR_CRC`        | Sensor CRC/checksum error      |
| `3`  | `SLAVE_ERR_SENSOR_RANGE`      | Sensor value out of range      |
| `4`  | `SLAVE_ERR_CONFIG_RUNTIME`    | Runtime config apply failed    |
| `5`  | `SLAVE_ERR_BAD_ADDRESS`       | Invalid assigned address       |
| `6`  | `SLAVE_ERR_UNSUPPORTED_WRITE` | Write to unsupported register  |
| `7`  | `SLAVE_ERR_BUSY`              | Device temporarily busy        |
| `8`  | `SLAVE_ERR_RECOVERY_MAC`      | Recovery MAC did not match     |
| `9`  | `SLAVE_ERR_IR_COMMAND_FAILED` | Last IR command failed         |

Modbus exception mapping:

| Condition                       | Modbus Exception            |
| ------------------------------- | --------------------------- |
| Unsupported function            | `0x01` Illegal Function     |
| Unknown register                | `0x02` Illegal Data Address |
| Invalid written value           | `0x03` Illegal Data Value   |
| Internal failure                | `0x04` Slave Device Failure |

## Suggested Header Config for Slave Firmware

```cpp
#pragma once

#include <Arduino.h>

#define SB_MODBUS_BAUDRATE 19200
#define SB_MODBUS_DEFAULT_ADDR 247
#define SB_MODBUS_PAIRING_ADDR 247
#define SB_MODBUS_MIN_ADDR 2
#define SB_MODBUS_MAX_ADDR 246

#define SB_PROTOCOL_VERSION 210

#define REG_NODE_ADDRESS              0x0000
#define REG_FW_VERSION                0x0001
#define REG_MAC_0_1                   0x0002
#define REG_MAC_2_3                   0x0003
#define REG_MAC_4_5                   0x0004

#define REG_TEMP_ASSIGNMENT           0x0010
#define REG_LUX_ASSIGNMENT            0x0011
#define REG_CO2_COUNT                 0x0012
#define REG_PRESENCE_ASSIGNMENT       0x0013
#define REG_RELAY_ASSIGNMENT          0x0014
#define REG_IR_PROJECTOR_ENABLE       0x0015
#define REG_IR_AC_1_ENABLE            0x0016
#define REG_IR_AC_2_ENABLE            0x0017

#define REG_CONFIG_VERSION            0x00F0
#define REG_LAST_ERROR                0x00F1
#define REG_UPTIME_LOW                0x00F2
#define REG_UPTIME_HIGH               0x00F3
#define REG_RECOVERY_MAC_0_1          0x00F4
#define REG_RECOVERY_MAC_2_3          0x00F5
#define REG_RECOVERY_MAC_4_5          0x00F6
#define REG_RECOVERY_NODE_ADDRESS     0x00F7

#define REG_TEMP_1_X10                0x0100
#define REG_TEMP_2_X10                0x0101
#define REG_TEMP_3_X10                0x0102
#define REG_TEMP_4_X10                0x0103
#define REG_LUX_1_LX                  0x0104
#define REG_LUX_2_LX                  0x0105
#define REG_LUX_3_LX                  0x0106
#define REG_LUX_4_LX                  0x0107
#define REG_CO2_PPM                   0x0108
#define REG_PRESENCE_1_STATE          0x0109
#define REG_PRESENCE_2_STATE          0x010A
#define REG_PRESENCE_3_STATE          0x010B
#define REG_PRESENCE_4_STATE          0x010C
#define REG_RELAY_1_STATE             0x010D
#define REG_RELAY_2_STATE             0x010E

#define REG_AC_1_POWER                0x0200
#define REG_AC_1_SET_TEMP             0x0201
#define REG_AC_1_MODE                 0x0202
#define REG_AC_2_POWER                0x0203
#define REG_AC_2_SET_TEMP             0x0204
#define REG_AC_2_MODE                 0x0205
#define REG_AC_1_COMMAND_STATUS       0x0206
#define REG_AC_2_COMMAND_STATUS       0x0207
#define REG_AC_1_FAN_SPEED            0x0208
#define REG_AC_1_SWING_VERTICAL       0x0209
#define REG_AC_1_SWING_HORIZONTAL     0x020A
#define REG_AC_2_FAN_SPEED            0x020B
#define REG_AC_2_SWING_VERTICAL       0x020C
#define REG_AC_2_SWING_HORIZONTAL     0x020D
#define REG_PROJECTOR_POWER           0x0210
#define REG_PROJECTOR_INPUT           0x0211
#define REG_PROJECTOR_COMMAND_STATUS  0x0212
```

## Suggested Runtime Data Model

```cpp
struct SlaveIdentity {
  uint8_t mac[6];
  uint16_t fw_version;
};

struct SlaveCapability {
  uint8_t temp_assignment_mask;
  uint8_t lux_assignment_mask;
  uint8_t co2_count;
  uint8_t presence_assignment_mask;
  uint8_t relay_assignment_mask;
  uint8_t ir_projector_enable;
  uint8_t ir_ac_1_enable;
  uint8_t ir_ac_2_enable;
};

struct SlaveRuntime {
  uint8_t active_address;
  bool waiting_at_pairing_address;
  uint16_t last_error;
  int16_t temp_x10[4];
  uint16_t lux_lx[4];
  uint16_t co2_ppm;
  uint16_t presence_state[4];
  uint16_t relay_state[2];
  uint16_t ac_1_command_status;
  uint16_t ac_2_command_status;
  uint16_t projector_command_status;
};
```

## Master Polling Pattern

Master will generally do:

```text
Read 0x0000 length 5      identity
Read 0x0010 length 8      capability assignment/count registers
Read 0x0100 length 15     sensors/actuator state block
Write 0x010D / 0x010E     relay controls when profile/assignment allows
Write 0x0200..0x020D      AC controls when IR_COMBO_NODE profile allows
Write 0x0210..0x0211      projector controls when IR_COMBO_NODE profile allows
```

During user-initiated pairing:

```text
Read 247:0x0000 length 5
User selects device profile/name/room/capability
Master writes 247:0x0010 length 8
Master writes 247:0x0000 new_address
Master stores MAC/address/profile/name/room in persistent registry
```

During automatic recovery for known offline device:

```text
Master loads registry
Known assigned address does not respond
Master writes 247:0x00F4 length 4
Master ignores response collision/error for this recovery write only
Master polls recovered assigned address
If recovered address responds, device becomes online
```

## Implementation Rules

Slave SHALL:

- Never transmit unless answering a Modbus request.
- Keep register reads fast and non-blocking.
- Cache sensor values in background.
- Not read slow sensors inside Modbus callback if it can block.
- Return last known sensor value if sensor update is in progress.
- Use invalid sentinel values if sensor is unavailable.
- Validate assigned address before accepting it.
- Compare the full recovery MAC before applying recovery address.
- Store address and capabilities in RAM only.
- Remain policy-blind.

Slave SHALL NOT:

- Decide dashboard mapping.
- Decide room role.
- Decide device name.
- Decide device profile.
- Decide capability policy.
- Know building structure.
- Push spontaneous RS485 messages.
- Use address `1`.
- Use address `247` as assigned normal address.
- Persist address or capability using EEPROM, Preferences, NVS, or filesystem.

Master SHALL:

- Own persistent device registry.
- Own address uniqueness.
- Own device profile policy.
- Own unknown-device pairing decisions.
- Auto recover known offline devices when appropriate.
- Not auto pair unknown devices.
- Confirm recovery by polling the recovered assigned address.

## Current Master Firmware Compatibility

This v2.1.0 contract is a breaking wire-contract revision from `V_1_4_0`.

Minimum v2.1.0 constants expected by master:

```cpp
#define RS485_MODBUS_PAIRING_ADDR 247
#define RS485_MODBUS_REG_NODE_ADDRESS 0x0000
#define RS485_MODBUS_REG_CONFIG_VERSION 0x00F0
#define RS485_MODBUS_REG_LAST_ERROR 0x00F1
#define RS485_MODBUS_REG_RECOVERY_MAC_0_1 0x00F4
#define RS485_MODBUS_REG_RECOVERY_MAC_2_3 0x00F5
#define RS485_MODBUS_REG_RECOVERY_MAC_4_5 0x00F6
#define RS485_MODBUS_REG_RECOVERY_NODE_ADDRESS 0x00F7
```

`SAVE_CONFIG` is removed from v2.1.0 because slave address and capability persistence belong to master, while slave remains RAM-only.

## Open Items

- Update master implementation to use v2.1.0 register map.
- Keep UIUX, connectivity mapping, MQTT/app, FSD, README, and architecture docs aligned with this v2.1.0 contract.
- Confirm final device profile enum names used in master UI and registry.
- Confirm command status timeout behavior for IR AC/projector commands.
- Confirm whether Lux 1-4 is physically supported by every profile or only selected hardware variants.

## Changelog

### v2.1.0

- Fixed v2.0.1 register conflicts.
- Locked final v2.1.0 register map.
- `NODE_ADDRESS` remains at `0x0000`.
- `CONFIG_VERSION` is `0x00F0`.
- `LAST_ERROR` is `0x00F1`.
- Recovery registers are locked to `0x00F4..0x00F7`.
- Removed `SAVE_CONFIG` from v2.1.0.
- Clarified master persistent storage ownership.
- Clarified slave RAM-only behavior.
- Split persistent registry fields from runtime `last_seen` and `online_status`.
- Clarified automatic recovery for known devices.
- Clarified unknown-device pairing must be user initiated.
- Replaced one-main-capability enforcement with master-owned device profile enforcement.
- Added `IR_COMBO_NODE` production exception.
- Added AC/projector command status registers.
- Clarified slave is policy-blind and only executes master writes.

### v2.0.1

- Drafted recovery flow update to avoid reading all slave MACs at address `247`.
- Introduced single recovery write transaction idea.
- Had conflicting recovery register numbering; superseded by v2.1.0.

### v2.0.0

- Drafted MAC-only identity model.
- Drafted contiguous sensor register block.
- Drafted capability assignment bitmasks.
- Had unresolved compatibility and numbering conflicts; superseded by v2.1.0.
