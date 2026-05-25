# RS485 Modbus Slave Firmware Contract

## Purpose

Dokumen ini adalah kontrak implementasi firmware slave untuk Smart Building RS485 Modbus.

Master firmware saat ini sudah memakai:

- Modbus RTU
- `DFRobot_RTU` sebagai Modbus master library
- UART0 board signal `TXD0/RXD0`
- MAX3485 half-duplex direction pin di master: `COM_SW = GPIO21`
- Baudrate `19200`
- Format serial `8N1`

Dokumen ini dipakai supaya firmware slave punya register map, default config, error convention, dan behavior yang sama dengan master.

## Master Requirement Summary

Master butuh slave dapat melakukan hal berikut:

- **All slaves boot with default address `247`.** This is the universal pairing/discovery address.
- Menjawab Modbus Holding Register read (`0x03`) untuk memberikan MAC address ke master.
- Menerima Modbus Holding Register write single (`0x06`) dan multiple (`0x10`) untuk config/control.
- Expose identity registers (MAC address, firmware version, etc.).
- Expose capability assignment/count registers.
- Expose sensor registers sesuai capability yang di-assign master.
- Menerima address assignment dan capability assignment dari master.
- After master assigns a new address (2-246), slave switches to that address and leaves pairing mode.
- Menyimpan address & capability HANYA di variable runtime (RAM), tanpa EEPROM.

Master tidak butuh slave tahu dashboard slot, room role, AC purpose, atau mapping UI.

## Recommended Slave Library

Pakai emelianov/modbus-esp8266@^4.1.0

Alasan:

- Mendukung ESP32.
- Mendukung Modbus RTU.
- Bisa bertindak sebagai Modbus slave/server.
- Cocok untuk expose holding register table.


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

Slave PCB note (Wemos Lolin C3 Mini test):

- RX = GPIO 20
- TX = GPIO 21
- COM_SW / RS485_DIR = GPIO 2

Address:

```cpp
#define MODBUS_ADDR_RESERVED_MASTER 1
#define MODBUS_ADDR_MIN_NORMAL      2
#define MODBUS_ADDR_MAX_NORMAL      246
#define MODBUS_ADDR_PAIRING         247
```

Note:

- Address `247` is reserved for pairing/default mode.
- Normal saved address must be `2..246`.
- Address `1` is reserved by system convention.

## Required Modbus Function Codes

Slave SHALL support responding to:

| Function | Name | Required |
|---|---|---|
| `0x01` | Read coils | Yes |
| `0x03` | Read Holding Registers | Yes |
| `0x04` | Read input Registers | Yes |
| `0x06` | Write Single Holding Register | Yes |
| `0x10` | Write multiple Holding Register | Yes |


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
int16_t temp_x10 = (int16_t)input_register[0x0100];
```

## Identity Registers

Base: `0x0000`

| Register | Name | Type | Required | Description |
|---|---|---|---|---|
| `0x0000` | `NODE_ADDRESS` | `uint16_t` | Yes | Current or new Modbus address |
| `0x0001` | `FW_VERSION` | `uint16_t` | Yes | Example `100` for v1.0.0 |
| `0x0002` | `MAC_0_1` | `uint16_t` | yes | MAC bytes 0 and 1 |
| `0x0003` | `MAC_2_3` | `uint16_t` | yes | MAC bytes 2 and 3 |
| `0x0004` | `MAC_4_5` | `uint16_t` | yes | MAC bytes 4 and 5 |

## Capability Registers

Base: `0x0010`

These registers are written by the Master during pairing to assign the hardware quantities managed by the Slave. A value of `0` means the capability is disabled.

| Register | Name | Access | Required | Description |
|---|---|---|---|---|
| `0x0010` | `TEMP_SENSOR_ASSIGNMENT` | R/W | Yes | Bitmask assignment for Temp 1-4. Read from MSB of a 4-bit nibble. |
| `0x0011` | `LUX_SENSOR_ASSIGNMENT` | R/W | Yes | Bitmask assignment for Lux 1-4. Read from MSB of a 4-bit nibble. |
| `0x0012` | `CO2_SENSOR_COUNT` | R/W | Yes | How many CO2 sensors. |
| `0x0013` | `PRESENCE_SENSOR_ASSIGNMENT` | R/W | Yes | Bitmask assignment for Presence 1-4. Read from MSB of a 4-bit nibble. |
| `0x0014` | `RELAY_ASSIGNMENT` | R/W | Yes | Bitmask assignment for Relay 1-2. Read from MSB of a 2-bit nibble. |
| `0x0015` | `IR_PROJECTOR_ENABLE` | R/W | Yes | `0` disable, `1` enable IR for Projector. |
| `0x0016` | `IR_AC_1_ENABLE` | R/W | Yes | `0` disable, `1` enable IR for AC 1. |
| `0x0017` | `IR_AC_2_ENABLE` | R/W | Yes | `0` disable, `1` enable IR for AC 2. |

Temperature Sensor Assignment Logic:

- The master assigns specific temperature sensors using a 4-bit mask in `0x0010`.
- Bit `3` (value 8): Temp 1
- Bit `2` (value 4): Temp 2
- Bit `1` (value 2): Temp 3
- Bit `0` (value 1): Temp 4
- Example `0x0003` (binary `0011`) assigns Temp 3 and 4.
- Example `0x0005` (binary `0101`) assigns Temp 2 and 4.
- `0x0000` disables temperature reading.

Lux Sensor Assignment Logic:

- The master assigns specific lux sensors using a 4-bit mask in `0x0011`.
- Bit `3` (value 8): Lux 1
- Bit `2` (value 4): Lux 2
- Bit `1` (value 2): Lux 3
- Bit `0` (value 1): Lux 4
- Example `0x0006` (binary `0110`) assigns Lux 2 and 3.
- `0x0000` disables lux reading.

Presence Sensor Assignment Logic:

- The master assigns specific presence sensors using a 4-bit mask in `0x0013`.
- Bit `3` (value 8): Presence 1
- Bit `2` (value 4): Presence 2
- Bit `1` (value 2): Presence 3
- Bit `0` (value 1): Presence 4
- `0x0000` disables presence reading.

Relay Assignment Logic:

- The master assigns specific relays using a 2-bit mask in `0x0014`.
- Bit `1` (value 2): Relay 1
- Bit `0` (value 1): Relay 2
- `0x0000` disables relay control.

IR Enable Logic:

- `IR_PROJECTOR_ENABLE` (`0x0015`): `0` disable, `1` enable IR for Projector.
- `IR_AC_1_ENABLE` (`0x0016`): `0` disable, `1` enable IR for AC 1.
- `IR_AC_2_ENABLE` (`0x0017`): `0` disable, `1` enable IR for AC 2.

Rules:

- A capability is fully determined by its assigned bitmask or count.

## Pairing / Config Registers

These registers are now the master-slave contract for recovery and config control. (`NODE_ADDRESS` is in Identity register `0x0000`.)

Base: `0x00F0`

| Register | Name | Access | Required | Description |
|---|---|---|---|---|
| `0x00F0` | `SAVE_CONFIG` | W | Optional | Write `0xA55A` as compatibility/config signal |
| `0x00F1` | `CONFIG_VERSION` | R | Recommended | Increment if config layout changes |
| `0x00F2` | `LAST_ERROR` | R/W | Recommended | Last slave error code, write `0` to clear |
| `0x00F3` | `UPTIME_LOW` | R | Optional | Uptime lower 16 bit seconds |
| `0x00F4` | `UPTIME_HIGH` | R | Optional | Uptime upper 16 bit seconds |
| `0x00F5` | `RECOVERY_MAC_0_1` | W | yes | MAC bytes 0 and 1 for recovery matching |
| `0x00F6` | `RECOVERY_MAC_2_3` | W | yes | MAC bytes 2 and 3 for recovery matching |
| `0x00F7` | `RECOVERY_MAC_4_5` | W | yes | MAC bytes 4 and 5 for recovery matching |
| `0x00F8` | `RECOVERY_NODE_ADDRESS` | W | yes | storing temp node address |


Required pairing behavior (RAM-only workflow):

1. **Boot:** All slaves start listening on the universal default address `247` (pairing mode).
2. **Discovery:** Master reads identity registers (MAC address, firmware version) from address `247`.
3. **Capability Assignment:** Master writes hardware capability quantities or assignment registers (`0x0010` to `0x0017`) at slave's address at that time ('247' if in pairing mode, or in slave's new assigned address).
4. **Address Assignment:** Master writes a new unique address (2-246) to register `0x0000` at address `247`.
5. **Transition:** Slave switches from address `247` to the newly assigned address and exits pairing mode.
6. **Master Persistence:** Master stores the MAC address + assigned address mapping in EEPROM for recovery.
7. **Slave Persistence:** All config (address, capabilities) is stored in RAM only; slave DOES NOT save to EEPROM.

Recommended:

- Apply new address immediately after assignment.
- Enable sensor/actuator logic only for capabilities with non-zero assignments or counts.

## Recovery / Re-pairing Mechanism

When the system reboots:
- All slaves revert to default address `247` (since they store config in RAM only).
- Master remembers the MAC → Address mappings stored in its EEPROM.

**Recovery Procedure:**

1. Master polls address `247` to all slaves present.
2. Master sends a recovery command at address `247` with:
  - The target slave's MAC address (registers `0x00F5` to `0x00F7`)
  - The saved address to apply (register `0x00F8`)
3. All slaves receive the recovery broadcast at address `247`.
4. Each slave compares the sent MAC with its own MAC.
5. **If MAC matches:** Slave applies the provided address (from `0x00F8`) and switches to it.
6. **If MAC does not match:** Slave ignores the command and remains at address `247`.

This allows the master to efficiently restore all slaves to their pre-reboot addresses without manual intervention.

## Sensor Registers

These registers expose sequential data for the master to read.

Master read recommendation:

- Read all sensor/actuator registers in one transaction: `0x0100` length `15` (covers `0x0100` to `0x010E`).
- The master should still apply assignment masks and invalid/unassigned sentinel rules when parsing each value.

| Register | Name | Type | Unit |
|---|---|---|---|
| `0x0100` | `TEMP_1_X10` | `int16_t` | Celsius x10 |
| `0x0101` | `TEMP_2_X10` | `int16_t` | Celsius x10 |
| `0x0102` | `TEMP_3_X10` | `int16_t` | Celsius x10 |
| `0x0103` | `TEMP_4_X10` | `int16_t` | Celsius x10 |
| `0x0104` | `LUX_1_LX` | `uint16_t` | lux |
| `0x0105` | `LUX_2_LX` | `uint16_t` | lux |
| `0x0106` | `LUX_3_LX` | `uint16_t` | lux |
| `0x0107` | `LUX_4_LX` | `uint16_t` | lux |
| `0x0108` | `CO2_PPM` | `uint16_t` | ppm |
| `0x0109` | `PRESENCE_1_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010A` | `PRESENCE_2_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010B` | `PRESENCE_3_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010C` | `PRESENCE_4_STATE` | `uint16_t` | `0` no person, `1` person present |
| `0x010D` | `RELAY_1_STATE` | `uint16_t` | `0` off, `1` on |
| `0x010E` | `RELAY_2_STATE` | `uint16_t` | `0` off, `1` on |

Sensor Invalid Values and Assignment Rules:

| Condition | Modbus Type | Value |
|---|---|---|
| Sensor reading error | Signed `int16_t` | `-32768` (`0x8000`) |
| Sensor not assigned | Signed `int16_t` | `-32767` (`0x8001`) |
| Sensor reading error | Unsigned `uint16_t` | `0xFFFF` (`65535`) |
| Sensor not assigned | Unsigned `uint16_t` | `0xFFFE` (`65534`) |

Master documentation note: If a value matches "sensor not assigned" it should be ignored. If it matches "reading error" the master can flag the sensor as offline.

## Control Registers

AC 1:

| Register | Name | Access | Values |
|---|---|---|---|
| `0x0200` | `AC_1_POWER` | R/W | `0` off, `1` on |
| `0x0201` | `AC_1_SET_TEMP` | R/W | Celsius x10 |
| `0x0202` | `AC_1_MODE` | R/W | slave-defined enum |

AC 2:

| Register | Name | Access | Values |
|---|---|---|---|
| `0x0203` | `AC_2_POWER` | R/W | `0` off, `1` on |
| `0x0204` | `AC_2_SET_TEMP` | R/W | Celsius x10 |
| `0x0205` | `AC_2_MODE` | R/W | slave-defined enum |

Projector:

| Register | Name | Access | Values |
|---|---|---|---|
| `0x0210` | `PROJECTOR_POWER` | R/W | `0` off, `1` on |
| `0x0211` | `PROJECTOR_INPUT` | R/W | slave-defined enum |



## Error Registers and Codes

Recommended slave error register:

```cpp
#define REG_LAST_ERROR 0x00F3
```

Error code values:

| Code | Name | Meaning |
|---|---|---|
| `0` | `SLAVE_ERR_NONE` | No error |
| `1` | `SLAVE_ERR_SENSOR_TIMEOUT` | Sensor read timeout |
| `2` | `SLAVE_ERR_SENSOR_CRC` | Sensor CRC/checksum error |
| `3` | `SLAVE_ERR_SENSOR_RANGE` | Sensor value out of range |
| `4` | `SLAVE_ERR_CONFIG_SAVE` | Preferences/EEPROM save failed |
| `5` | `SLAVE_ERR_BAD_ADDRESS` | Invalid assigned address |
| `6` | `SLAVE_ERR_UNSUPPORTED_WRITE` | Write to unsupported register |
| `7` | `SLAVE_ERR_BUSY` | Device temporarily busy |

Modbus exception mapping:

| Condition | Modbus Exception |
|---|---|
| Unsupported function | `0x01` Illegal Function |
| Unknown register | `0x02` Illegal Data Address |
| Invalid written value | `0x03` Illegal Data Value |
| Save failure / internal failure | `0x04` Slave Device Failure |

## Suggested Header Config for Slave Firmware

```cpp
#pragma once

#include <Arduino.h>

#define SB_MODBUS_BAUDRATE 19200
#define SB_MODBUS_DEFAULT_ADDR 247    // Universal pairing/discovery address
#define SB_MODBUS_PAIRING_ADDR 247    // Alias for default
#define SB_MODBUS_MIN_ADDR 2          // Min assigned address
#define SB_MODBUS_MAX_ADDR 246        // Max assigned address

#define SB_PROTOCOL_VERSION 1
#define SB_SAVE_CONFIG_VALUE 0xA55A

#define REG_NODE_ADDRESS      0x0000
#define REG_FW_VERSION        0x0001
#define REG_MAC_0_1           0x0002
#define REG_MAC_2_3           0x0003
#define REG_MAC_4_5           0x0004

#define REG_TEMP_ASSIGNMENT   0x0010
#define REG_LUX_ASSIGNMENT    0x0011
#define REG_CO2_COUNT         0x0012
#define REG_PRESENCE_ASSIGNMENT 0x0013
#define REG_RELAY_ASSIGNMENT  0x0014
#define REG_IR_PROJECTOR_ENABLE 0x0015
#define REG_IR_AC_1_ENABLE    0x0016
#define REG_IR_AC_2_ENABLE    0x0017

#define REG_SAVE_CONFIG       0x00F0
#define REG_CONFIG_VERSION    0x00F1
#define REG_LAST_ERROR        0x00F2
#define REG_UPTIME_LOW        0x00F3
#define REG_UPTIME_HIGH       0x00F4
#define REG_RECOVERY_MAC_0_1  0x00F5
#define REG_RECOVERY_MAC_2_3  0x00F6
#define REG_RECOVERY_MAC_4_5  0x00F7
#define REG_RECOVERY_NODE_ADDRESS  0x00F8

#define REG_TEMP_1_X10        0x0100
#define REG_TEMP_2_X10        0x0101
#define REG_TEMP_3_X10        0x0102
#define REG_TEMP_4_X10        0x0103
#define REG_LUX_1_LX          0x0104
#define REG_LUX_2_LX          0x0105
#define REG_LUX_3_LX          0x0106
#define REG_LUX_4_LX          0x0107
#define REG_CO2_PPM           0x0108
#define REG_PRESENCE_1_STATE  0x0109
#define REG_PRESENCE_2_STATE  0x010A
#define REG_PRESENCE_3_STATE  0x010B
#define REG_PRESENCE_4_STATE  0x010C
#define REG_RELAY_1_STATE     0x010D
#define REG_RELAY_2_STATE     0x010E

#define REG_AC_1_POWER        0x0200
#define REG_AC_1_SET_TEMP     0x0201
#define REG_AC_1_MODE         0x0202
#define REG_AC_2_POWER        0x0203
#define REG_AC_2_SET_TEMP     0x0204
#define REG_AC_2_MODE         0x0205
#define REG_PROJECTOR_POWER   0x0210
#define REG_PROJECTOR_INPUT   0x0211
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
    uint8_t saved_address;
    bool pairing_mode;
    uint32_t pairing_started_ms;
    uint16_t last_error;
    int16_t temp_x10[4];
    uint16_t lux_lx[4];
    uint16_t co2_ppm;
    uint16_t presence_state[4];
    uint16_t relay_state[2];
};
```

## Slave Pairing State Machine

```text
BOOT (DEFAULT / PAIRING MODE)
  active_address = 247
  listening_address = 247
  expose identity registers (MAC, FW version) on address 247
  waiting for master pairing commands

PAIRING_RX_CAPABILITY
  master writes quantities to 0x0010 - 0x0017 at address 247
  store assignments and counts in RAM
  enable specific sensor reading based on assignment or count
  still listening on address 247

PAIRING_RX_ADDRESS
  master writes 0x0000 with new address at address 247
  validate new_address in range 2..246
  store new_address in RAM
  active_address = new_address
  listening_address = new_address
  EXIT PAIRING MODE
  now listening only on assigned address

RECOVERY_MODE (After system reboot)
  active_address = 247 (reverted because stored only in RAM)
  listening_address = 247
  waiting for master recovery signal
  
RECOVERY_RX_MAC_AND_ADDRESS
  master writes MAC bytes to 0x00F5, 0x00F6, 0x00F7
  master writes recovery address to 0x00F8 at address 247
  slave compares received MAC with own MAC
  if MAC matches:
    active_address = value from 0x00F8
    listening_address = active_address
    EXIT RECOVERY MODE
    resume normal operation at assigned address
  if MAC does not match:
    ignore command
    remain at address 247
```

## Master Polling Pattern

Master will generally do:

```text
Read 0x0000 length 5      identity (Address, Firmware, MAC)
Read 0x0010 length 8      capability assignment/counts
Read 0x0100 length 15     sensors/actuators block (0x0100..0x010E)
Write 0x010D / 0x010E     relay controls (based on RELAY_ASSIGNMENT)
```

During pairing:

```text
Read 247:0x0000 length 5     (Discovery: Read MAC and identity)
Write 247:0x0010 length 8    (Assign capability counts)
Write 247:0x0000 new_address (Assign new address, exit pairing mode)
Write 247:0x00F0 0xA55A      (Optional: Save config signal)
```

During recovery / re-pairing (system reboot - slaves reverted to 247):

```text
Read 247:0x0000 length 5     (Discovery: Read all MACs currently at address 247)

For each discovered MAC:
  Lookup MAC in master's EEPROM database to get saved_address
  Write 247:0x00F5 length 3   (Write recovered MAC to 0x00F5, 0x00F6, 0x00F7)
  Write 247:0x00F8 saved_address (Write recovered address to 0x00F8)
  
  Result:
    - Matching slave: Compares MAC, matches, applies saved_address, exits recovery
    - Non-matching slaves: Ignore, remain at address 247
```

Note: After address assignment in either pairing or recovery, the slave switches from address 247 to its assigned address and is no longer listening on 247 until next reboot.

## Implementation Rules

Slave SHALL:

- Never transmit unless answering a Modbus request.
- Keep register reads fast and non-blocking.
- Cache sensor values in background; do not read slow sensors inside Modbus callback if it can block.
- Return last known sensor value if sensor update is in progress.
- Use invalid sentinel values if sensor is unavailable.
- Validate assigned address before accepting it.
- **During recovery:** Compare the received MAC (from registers 0x00F5-0x00F7) with its own MAC. Only apply the recovery address if the MAC matches exactly.
- DO NOT use EEPROM to persist config; all config is kept in runtime variables (RAM only).

Slave SHOULD:

- Update sensors in its own task/timer.
- Keep Modbus server loop very frequent.
- Provide `LAST_ERROR`.
- Clear `LAST_ERROR` when master writes `0`.

Slave SHALL NOT:

- Decide dashboard mapping.
- Decide room role.
- Push spontaneous RS485 messages.
- Use address `1`.
- Use address `247` as saved normal address.

## Current Master Firmware Compatibility

Current master expects these constants:

```cpp
#define RS485_MODBUS_PAIRING_ADDR 247
#define RS485_MODBUS_REG_NODE_ADDRESS 0x0000
#define RS485_MODBUS_REG_SAVE_CONFIG  0x00F0
#define RS485_MODBUS_SAVE_CONFIG_VALUE 0xA55A
```

If the slave implements this file, the master pairing flow can work without changing master code.

## Open Items

- Persistent registry storage on master is not implemented yet.
- Dashboard mapping UI is not implemented yet.
- Slave duplicate WRITE protection is not required yet.
- If future slave library exposes sensor registers as input registers (`0x04`) instead of holding registers (`0x03`), master must add function-code mapping.

## Changelog

**v2.0.0**
- Removed `DEVICE_MAGIC`, `UID`, and device class values. MAC is the sole identity.
- Moved `NODE_ADDRESS` into Identity register `0x0000`.
- Capability temp sensors now use `TEMP_SENSOR_ASSIGNMENT` bitmask at `0x0010`.
- Added `LUX_SENSOR_ASSIGNMENT` and Lux 1-4 registers; presence and relay now use assignment masks.
- Sensor registers are contiguous; humidity removed; presence expanded to 4.
- Master reads sensor/actuator block in one transaction (`0x0100` length `15`).
- Removed LCD control capability; IR enable is separate for Projector, AC 1, and AC 2.
- Some register numbering changed to remove gaps after deletions. There will be some mitchmatch of the registor numbering in the previous changelog version
- Added explicit invalid and unassigned sentinel values for master parsing.

**v1.4.0**
- **Master persistent storage:** Master now saves MAC address → assigned address mappings in its EEPROM.
- **Recovery / re-pairing mechanism:** Added registers for MAC-based address recovery (0x00F6-0x00F9).
- **Recovery procedure:** When system reboots and slaves revert to address 247, master can recover all slaves by:
  1. Reading MACs from all slaves at address 247
  2. Looking up each MAC in its EEPROM database
  3. Broadcasting recovery commands with MAC + saved address
  4. Matching slaves apply their saved address and exit recovery mode
- New registers: `RECOVERY_MAC_0_1` (0x00F6), `RECOVERY_MAC_2_3` (0x00F7), `RECOVERY_MAC_4_5` (0x00F8), `RECOVERY_ADDRESS` (0x00F9)
- Updated Slave Pairing State Machine to include recovery states

**v1.3.0**
- **Default address concept formalized:** All slaves boot with address `247` as the universal pairing/discovery address.
- **Pairing workflow simplified:** Master discovers slave by reading MAC at address `247`, assigns capability counts, then assigns a unique address (2-246) to move slave out of pairing mode.
- Clarified that address assignment (`0x00F0` write) is the mechanism for exiting pairing mode and transitioning to normal operation.
- Updated Slave Pairing State Machine to show transition from pairing to assigned address.

**v1.2.0**
- Capability assignment updated: Master writes exact quantities to count registers (`0x0011` to `0x0016`) instead of using a capability bitmask.
- Deprecated `CAPABILITY_MASK` register (`0x0010`). Capability is completely determined by checking if sensor counts are greater than 0.

**v1.1.0**
- Removed EEPROM requirement for slave. Master holds all persistent storage.
- Slave now boots directly to pairing address (247).
- Slave workflow updated: Boot at 247 -> Master requests MAC -> Master assigns Capability Mask -> Master assigns new address.
- Config is saved entirely in slave RAM instead of EEPROM/NVS.
- Added specific Wemos Lolin C3 Mini UART wiring pins.
