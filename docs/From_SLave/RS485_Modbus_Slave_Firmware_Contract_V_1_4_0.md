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
- Expose capability count registers.
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

Alternatif jika firmware slave memakai ESP-IDF native:

- ESP-Modbus dari Espressif.

Slave boleh memakai library lain selama kompatibel dengan:

- Modbus RTU
- function `0x03`
- function `0x06`
- 19200 baud 8N1
- half-duplex RS485 direction control

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

Slave SHALL support:

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
UID = (UID_HIGH << 16) | UID_LOW
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
| `0x0000` | `DEVICE_MAGIC` | `uint16_t` | Yes | Recommended value `0x5342` (`SB`) |


| `0x0005` | `FW_VERSION` | `uint16_t` | Yes | Example `100` for v1.0.0 |
| `0x0006` | `MAC_0_1` | `uint16_t` | yes | MAC bytes 0 and 1 |
| `0x0007` | `MAC_2_3` | `uint16_t` | yes | MAC bytes 2 and 3 |
| `0x0008` | `MAC_4_5` | `uint16_t` | yes | MAC bytes 4 and 5 |

Device class values:

| Value | Meaning |
|---|---|
| `1` | Generic capability node |
| `2` | Sensor-only node |
| `3` | Relay/control node |
| `4` | IR control node |
| `5` | Display/LCD control node |
| `6` | Multi-function room node |

## Capability Registers

Base: `0x0010`

These registers are written by the Master during pairing to assign the hardware quantities managed by the Slave. A count of `0` means the capability is disabled.

| Register | Name | Access | Required | Description |
|---|---|---|---|---|
| `0x0011` | `TEMP_SENSOR_COUNT` | R/W | Yes | How many temperature sensors to expose/poll. |
| `0x0012` | `CO2_SENSOR_COUNT` | R/W | Yes | How many CO2 sensors. |
| `0x0013` | `PRESENCE_SENSOR_COUNT` | R/W | Yes | How many presence sensors. |
| `0x0014` | `RELAY_COUNT` | R/W | Yes | How many relays. |
| `0x0015` | `IR_COUNT` | R/W | Yes | How many IR transmitters. |
| `0x0016` | `LCD_CTRL_COUNT` | R/W | Yes | How many LCD controllers. |

Rules:

- A capability is fully determined by its assigned count.
- If Master sets `TEMP_SENSOR_COUNT` to `3`, the slave must activate 3 temperature sensor logic paths.
- Master caps temperature count to max `4`.
- Master caps relay count to max `2`.

## Pairing / Config Registers

These registers are now the master-slave contract for pairing.

Base: `0x00F0`

| Register | Name | Access | Required | Description |
|---|---|---|---|---|
| `0x00F0` | `NODE_ADDRESS` | R/W | Yes | Current or new Modbus address |
| `0x00F1` | `SAVE_CONFIG` | W | Yes | Write `0xA55A` to persist config |
| `0x00F2` | `CONFIG_VERSION` | R | Recommended | Increment if config layout changes |
| `0x00F3` | `LAST_ERROR` | R/W | Recommended | Last slave error code, write `0` to clear |
| `0x00F4` | `UPTIME_LOW` | R | Optional | Uptime lower 16 bit seconds |
| `0x00F5` | `UPTIME_HIGH` | R | Optional | Uptime upper 16 bit seconds |
| `0x00F6` | `RECOVERY_MAC_0_1` | W | yes | MAC bytes 0 and 1 for recovery matching |
| `0x00F7` | `RECOVERY_MAC_2_3` | W | yes | MAC bytes 2 and 3 for recovery matching |
| `0x00F8` | `RECOVERY_MAC_4_5` | W | yes | MAC bytes 4 and 5 for recovery matching |
| `0x00F9` | `RECOVERY_node_address` | W | yes | storing temp node address|


Required pairing behavior (RAM-only workflow):

1. **Boot:** All slaves start listening on the universal default address `247` (pairing mode).
2. **Discovery:** Master reads identity registers (MAC address, firmware version) from address `247`.
3. **Capability Assignment:** Master writes hardware capability quantities to count registers (`0x0011` to `0x0016`) at address `247`.
4. **Address Assignment:** Master writes a new unique address (2-246) to register `0x00F0` at address `247`.
5. **Transition:** Slave switches from address `247` to the newly assigned address and exits pairing mode.
6. **Master Persistence:** Master stores the MAC address + assigned address mapping in EEPROM for recovery.
7. **Slave Persistence:** All config (address, capabilities) is stored in RAM only; slave DOES NOT save to EEPROM.

Recommended:

- Apply new address immediately after assignment.
- Enable sensor reading logic only for capabilities with counts greater than 0.

## Recovery / Re-pairing Mechanism

When the system reboots:
- All slaves revert to default address `247` (since they store config in RAM only).
- Master remembers the MAC → Address mappings stored in its EEPROM.

**Recovery Procedure:**

1. Master polls address `247` to all slaves present.
2. Master sends a recovery command at address `247` with:
   - The target slave's MAC address (registers `0x00F6` to `0x00F8`)
   - The saved address to apply (register `0x00F9`)
3. All slaves receive the recovery broadcast at address `247`.
4. Each slave compares the sent MAC with its own MAC.
5. **If MAC matches:** Slave applies the provided address (from `0x00F9`) and switches to it.
6. **If MAC does not match:** Slave ignores the command and remains at address `247`.

This allows the master to efficiently restore all slaves to their pre-reboot addresses without manual intervention.

## Sensor Registers

Temperature:

| Register | Name | Type | Unit |
|---|---|---|---|
| `0x0100` | `TEMP_1_X10` | `int16_t` | Celsius x10 |
| `0x0101` | `TEMP_2_X10` | `int16_t` | Celsius x10 |
| `0x0102` | `TEMP_3_X10` | `int16_t` | Celsius x10 |
| `0x0103` | `TEMP_4_X10` | `int16_t` | Celsius x10 |

Air quality:

| Register | Name | Type | Unit |
|---|---|---|---|
| `0x0110` | `CO2_PPM` | `uint16_t` | ppm |

| `0x0112` | `HUMIDITY_X10` | `uint16_t` | percent RH x10 |

Presence:

| Register | Name | Type | Unit |
|---|---|---|---|
| `0x0120` | `PRESENCE_STATE` | `uint16_t` | `0` no person, `1` person present |


Relay:

| Register | Name | Type | Unit |
|---|---|---|---|
| `0x0130` | `RELAY_1_STATE` | `uint16_t` | `0` off, `1` on |
| `0x0131` | `RELAY_2_STATE` | `uint16_t` | `0` off, `1` on |

Sensor invalid values:

| Sensor | Invalid Value |
|---|---|
| Temperature | `-32768` |
| CO2 | `0xFFFF` |
| Humidity | `0xFFFF` |
| Presence confidence | `0xFFFF` |

## Control Registers

AC:

| Register | Name | Access | Values |
|---|---|---|---|
| `0x0200` | `AC_POWER` | R/W | `0` off, `1` on |
| `0x0201` | `AC_SET_TEMP` | R/W | Celsius x10 |
| `0x0202` | `AC_MODE` | R/W | slave-defined enum |

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

#define SB_DEVICE_MAGIC 0x5342
#define SB_PROTOCOL_VERSION 1
#define SB_SAVE_CONFIG_VALUE 0xA55A

#define REG_DEVICE_MAGIC      0x0000
#define REG_PROTOCOL_VERSION  0x0001

#define REG_FW_VERSION        0x0005
#define REG_MAC_0_1           0x0006
#define REG_MAC_2_3           0x0007
#define REG_MAC_4_5           0x0008

#define REG_TEMP_COUNT        0x0011
#define REG_CO2_COUNT         0x0012
#define REG_PRESENCE_COUNT    0x0013
#define REG_RELAY_COUNT       0x0014
#define REG_IR_COUNT          0x0015
#define REG_LCD_COUNT         0x0016

#define REG_NODE_ADDRESS      0x00F0
#define REG_SAVE_CONFIG       0x00F1
#define REG_CONFIG_VERSION    0x00F2
#define REG_LAST_ERROR        0x00F3
#define REG_UPTIME_LOW        0x00F4
#define REG_UPTIME_HIGH       0x00F5
#define REG_RECOVERY_MAC_0_1  0x00F6
#define REG_RECOVERY_MAC_2_3  0x00F7
#define REG_RECOVERY_MAC_4_5  0x00F8
#define REG_RECOVERY_ADDRESS  0x00F9

#define REG_TEMP_1_X10        0x0100
#define REG_TEMP_2_X10        0x0101
#define REG_TEMP_3_X10        0x0102
#define REG_TEMP_4_X10        0x0103
#define REG_CO2_PPM           0x0110
#define REG_TVOC              0x0111
#define REG_HUMIDITY_X10      0x0112
#define REG_PRESENCE_STATE    0x0120
#define REG_PRESENCE_CONF     0x0121
#define REG_RELAY_1_STATE     0x0130
#define REG_RELAY_2_STATE     0x0131

#define REG_AC_POWER          0x0200
#define REG_AC_SET_TEMP       0x0201
#define REG_AC_MODE           0x0202
#define REG_PROJECTOR_POWER   0x0210
#define REG_PROJECTOR_INPUT   0x0211
#define REG_LCD_POWER         0x0220
#define REG_LCD_INPUT         0x0221
```

## Suggested Runtime Data Model

```cpp
struct SlaveIdentity {
    uint16_t magic;
    uint16_t protocol_version;
    uint16_t device_class;
    uint16_t fw_version;
    uint32_t uid;
    uint8_t mac[6];
};

struct SlaveCapability {
    uint8_t temp_count;
    uint8_t co2_count;
    uint8_t presence_count;
    uint8_t relay_count;
    uint8_t ir_count;
    uint8_t lcd_count;
};

struct SlaveRuntime {
    uint8_t active_address;
    uint8_t saved_address;
    bool pairing_mode;
    uint32_t pairing_started_ms;
    uint16_t last_error;
    int16_t temp_x10[4];
    uint16_t co2_ppm;
    uint16_t tvoc;
    uint16_t humidity_x10;
    uint16_t presence_state;
    uint16_t presence_confidence;
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
  master writes quantities to 0x0011 - 0x0016 at address 247
  store counts in RAM
  enable specific sensor reading based on > 0 counts
  still listening on address 247

PAIRING_RX_ADDRESS
  master writes 0x00F0 with new address at address 247
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
  master writes MAC bytes to 0x00F6, 0x00F7, 0x00F8
  master writes recovery address to 0x00F9 at address 247
  slave compares received MAC with own MAC
  if MAC matches:
    active_address = value from 0x00F9
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
Read 0x0000 length 9      identity
Read 0x0011 length 6      capability counts
Read 0x0100 length N      temperature (based on TEMP_SENSOR_COUNT)
Read 0x0110 length 3      air quality (if CO2_SENSOR_COUNT > 0)
Read 0x0120 length 2      presence (if PRESENCE_SENSOR_COUNT > 0)
Write 0x0130 / 0x0131     relay controls (if RELAY_COUNT > 0)
```

During pairing:

```text
Read 247:0x0000 length 9     (Discovery: Read MAC and identity)
Write 247:0x0011 length 6    (Assign capability counts)
Write 247:0x00F0 new_address (Assign new address, exit pairing mode)
Write 247:0x00F1 0xA55A      (Optional: Save config signal)
```

During recovery / re-pairing (system reboot - slaves reverted to 247):

```text
Read 247:0x0000 length 9     (Discovery: Read all MACs currently at address 247)

For each discovered MAC:
  Lookup MAC in master's EEPROM database to get saved_address
  Write 247:0x00F6 length 3   (Write recovered MAC to 0x00F6, 0x00F7, 0x00F8)
  Write 247:0x00F9 saved_address (Write recovered address to 0x00F9)
  
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
- **During recovery:** Compare the received MAC (from registers 0x00F6-0x00F8) with its own MAC. Only apply the recovery address if the MAC matches exactly.
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
#define RS485_MODBUS_REG_NODE_ADDRESS 0x00F0
#define RS485_MODBUS_REG_SAVE_CONFIG  0x00F1
#define RS485_MODBUS_SAVE_CONFIG_VALUE 0xA55A
```

If the slave implements this file, the master pairing flow can work without changing master code.

## Open Items

- Persistent registry storage on master is not implemented yet.
- Dashboard mapping UI is not implemented yet.
- Slave duplicate WRITE protection is not required yet.
- If future slave library exposes sensor registers as input registers (`0x04`) instead of holding registers (`0x03`), master must add function-code mapping.

## Changelog

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

