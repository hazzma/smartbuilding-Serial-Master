# Smart Building Connectivity & Dashboard Mapping Design

## Project
Smart Building Master S3

## Document Purpose

Dokumen ini mendefinisikan:
- Slave discovery architecture
- Slave identity model
- Capability model
- Dashboard logical mapping
- Data composition system
- Auto mapping behavior
- Manual mapping behavior
- IR control routing
- Dashboard NULL/stale policy
- UI flow dan orchestration

Tujuan utama:

> Membuat Master menjadi orchestrator data dan dashboard dinamis, bukan sekadar viewer slave.

---

# 1. Core Design Philosophy

Dashboard SHALL NOT membaca slave secara langsung.

Dashboard membaca:
- logical slot
- logical endpoint
- logical capability

Master bertanggung jawab melakukan mapping:

```text
Logical Slot
    ->
Slave
    ->
Capability
    ->
Channel
```

---

# 2. High-Level Architecture

```text
+--------------------------------------------------+
|                MASTER HMI                        |
|--------------------------------------------------|
| Dashboard                                        |
| Slave Manager                                    |
| Mapping Manager                                  |
| RS485 Manager                                    |
+--------------------------------------------------+
                    |
                    |
              RS485 BUS
                    |
    +---------------+---------------+
    |               |               |
+----------+  +-----------+  +-----------+
| TempNode |  | CO2Node   |  | IRNode    |
| Temp x4  |  | CO2       |  | AC/Proj   |
| +Lux opt |  | +Lux opt  |  | +Lux opt  |
+----------+  +-----------+  +-----------+
```

Firmware V2 note:

- What changed: examples now use the v2.1.0 Device Profile model, with Lux as an optional auxiliary capability and `IR_COMBO_NODE` as the intentional combined IR profile.
- Why it changed: master-owned Device Profiles prevent ambiguous assignments while allowing production IR hardware to expose AC 1, AC 2, and Projector together.
- Implementation effect: mapping should expect profile-driven slaves for Temperature, CO2, Presence, Relay/LED, and IR control; the master enforces profile policy while the slave remains policy-blind.

---

# 3. Slave Identity Model

Slave wajib memiliki:
- UID unik
- atau MAC unik

Address RS485 dapat berubah.

UID/MAC tidak boleh berubah.

UI boleh menampilkan:

```text
MAC: A1:B2:C3:D4:E5:F6
```

atau:

```text
UID: A1B2C3D4
```

---

# 4. Slave Registry Model

```cpp
struct SlaveDevice {
    uint8_t address;

    uint64_t mac;

    DeviceProfile profile;
    char name[24];
    char room[24];

    uint8_t temp_assignment_mask;
    uint8_t lux_assignment_mask;
    uint8_t co2_count;
    uint8_t presence_assignment_mask;
    uint8_t relay_assignment_mask;
    bool ir_projector_enable;
    bool ir_ac_1_enable;
    bool ir_ac_2_enable;
};
```

Saved registry fields SHALL be:

```text
MAC
Assigned address
Device Profile
Device name
Room
```

Runtime or low-frequency persistence fields:

```text
Last seen
Online/offline status
```

`last_seen` and `online` SHOULD NOT be written to persistent storage on every poll.

Master persistent registry is the source of truth. Slave address and capability values are RAM-only on the slave and are restored by pairing or recovery.

---

# 5. Capability Model

```cpp
enum DeviceProfile {
    TEMP_NODE,
    PRESENCE_NODE,
    CO2_NODE,
    RELAY_NODE,
    IR_COMBO_NODE
};

enum CapabilityBit {
    CAP_TEMP          = 1 << 0,
    CAP_CO2           = 1 << 1,
    CAP_HUMAN_PRESENCE= 1 << 2,
    CAP_AC_IR         = 1 << 3,
    CAP_PROJECTOR_IR  = 1 << 4,
    CAP_LIGHT_RELAY   = 1 << 5,
    CAP_LUX           = 1 << 6
};
```

Active slave contract:

```text
docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md
Version: v2.1.0
```

Firmware V2.1 capability rule:

- What changed: the master SHALL assign a Device Profile and write only the v2.1.0 capability registers allowed by that profile.
- Why it changed: the slave contract makes the master responsible for profile policy, room, naming, dashboard mapping, and capability decisions.
- Implementation effect: the UI selects `TEMP_NODE`, `PRESENCE_NODE`, `CO2_NODE`, `RELAY_NODE`, or `IR_COMBO_NODE`; capability rows are enabled or unavailable based on that selected profile.

Device Profile policy:

```text
TEMP_NODE       -> Temperature 1-4, optional Lux
PRESENCE_NODE   -> Presence 1-4, optional Lux
CO2_NODE        -> CO2, optional Lux
RELAY_NODE      -> Relay 1-2, optional Lux
IR_COMBO_NODE   -> AC 1, AC 2, Projector IR, optional Lux
```

`IR_COMBO_NODE` is an intentional production exception. AC 1, AC 2, and Projector may coexist on that profile.

v2.1.0 capability writes:

```text
0x0010 TEMP_SENSOR_ASSIGNMENT
0x0011 LUX_SENSOR_ASSIGNMENT
0x0012 CO2_SENSOR_COUNT
0x0013 PRESENCE_SENSOR_ASSIGNMENT
0x0014 RELAY_ASSIGNMENT
0x0015 IR_PROJECTOR_ENABLE
0x0016 IR_AC_1_ENABLE
0x0017 IR_AC_2_ENABLE
```

Slave remains policy-blind and RAM-only.

Legacy / V1 Notes:

- Older drafts allowed a slave to have many capabilities at once. That assumption is deprecated for Firmware V2 and should be kept only as historical context.

Human presence SHALL represent whether a person is present or not present.
It is not tied to a specific sensor technology name.
The slave may internally use any presence detection method, but the dashboard semantic is only:

```text
human_presence = true  -> ada orang
human_presence = false -> tidak ada orang
```

---

# 6. Dashboard Logical Slots

Dashboard membaca logical slot:

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

Dashboard tidak tahu slave mana sumber data.

---

# 7. Logical Mapping Model

```cpp
struct LogicalMapping {
    uint8_t logical_id;

    uint8_t capability_type;

    uint32_t slave_uid;
    uint8_t slave_addr;

    uint8_t channel;

    bool assigned;
    bool manual_override;
};
```

---

# 8. Example Mapping

```text
TEMP_SLOT_1 -> Slave A TEMP_1_X10 at 0x0100
TEMP_SLOT_2 -> Slave A TEMP_2_X10 at 0x0101
TEMP_SLOT_3 -> NULL
TEMP_SLOT_4 -> NULL

CO2_MAIN -> Slave B CO2_PPM at 0x0108
LUX_MAIN -> Slave B LUX_1_LX at 0x0104

AC_CONTROL -> Slave C IR_COMBO_NODE / AC 1+2 command registers
PROJECTOR_CONTROL -> Slave C IR_COMBO_NODE / Projector command registers
```

Runtime register block used by mapping examples:

```text
Sensor/state block: 0x0100..0x010E
Read 0x0100 length 15

0x0100..0x0103 -> TEMP_1_X10..TEMP_4_X10
0x0104..0x0107 -> LUX_1_LX..LUX_4_LX
0x0108         -> CO2_PPM
0x0109..0x010C -> PRESENCE_1_STATE..PRESENCE_4_STATE
0x010D..0x010E -> RELAY_1_STATE..RELAY_2_STATE
```

---

# 9. Dynamic Expansion

Jika slave pertama hanya punya:

```text
2 temperature sensor
```

Maka:

```text
TEMP_SLOT_1 = valid
TEMP_SLOT_2 = valid
TEMP_SLOT_3 = NULL
TEMP_SLOT_4 = NULL
```

Jika slave baru masuk dengan 2 sensor tambahan:

```text
TEMP_SLOT_3 = valid
TEMP_SLOT_4 = valid
```

Dashboard berkembang otomatis.

---

# 10. NULL / STALE Behavior

Jika slot tidak memiliki source:

```text
display "--"
```

Jika slave offline:

```text
mapping tetap ada
tetapi value menjadi stale/null
```

Dashboard tidak boleh menampilkan data lama sebagai valid.

---

# 11. Auto Mapping Rules

```text
1. Isi temperature slot kosong dari slave online yang memiliki sensor temperature.

2. Isi CO2_MAIN dari slave online pertama yang memiliki CO2 aktif.

3. Isi LUX_MAIN dari slave online yang memiliki Lux assignment aktif pada register v2.1.0 `0x0011`, dengan runtime Lux di `0x0104..0x0107`.

4. Isi HUMAN_PRESENCE_MAIN dari slave online pertama yang memiliki human presence aktif.

5. Isi AC_CONTROL dari slave online pertama yang memiliki AC IR capability aktif.

6. Isi PROJECTOR_CONTROL dari slave online pertama yang memiliki Projector IR aktif.

7. Manual override SHALL NOT ditimpa auto-map.
```

---

# 11.1 HMI Mode Selection Rule

HMI SHALL NOT require a switch between old assignment modes such as:

```text
1 slave = 1 sensor
```

and:

```text
1 slave = multiple sensors
```

Firmware V2.1 uses one assignment model:

```text
Device Profile + profile-allowed capability assignments
```

The UI SHALL adapt automatically based on selected Device Profile and enabled channels.

User-facing control SHALL be:

```text
Auto Map
Manual Mapping / Advanced
```

not:

```text
Single Sensor Mode
Multi Sensor Mode
```

What changed: the UI no longer exposes legacy assignment modes.
Why it changed: Firmware V2.1 uses master-owned Device Profiles and a policy-blind slave.
Implementation effect: after the user selects a Device Profile, the UI enables only the rows allowed by that profile. `IR_COMBO_NODE` enables AC 1, AC 2, and Projector together; Lux is optional when supported.

---

# 12. Dashboard Runtime Model

```cpp
struct DashboardModel {
    float temp[4];

    bool temp_valid[4];

    int co2;
    bool co2_valid;

    float lux;
    bool lux_valid;

    bool human_presence;
    bool human_presence_valid;

    bool ac_available;
    bool projector_available;
};
```

---

# 13. IR Control Philosophy

IR control adalah:

```text
Control Endpoint
```

bukan sensor.

Flow:

```text
Dashboard UI
    ->
Control Request
    ->
RS485 Manager
    ->
IR Slave
    ->
IR Transmitter
```

---

# 14. Slave Manager Page

Slave Manager adalah halaman overview untuk melihat semua slave yang dikenal master.

Halaman ini SHALL tetap ringkas dan tidak menampilkan semua detail konfigurasi dalam satu layar.

Primary function:
- lihat status bus
- lihat daftar slave
- masuk ke detail slave
- discover slave baru
- manual test cepat untuk slave terpilih

Current implemented base layout:

```text
+------------------------------------------------+
| SLAVE MANAGER                         RS485 OK |
+------------------------------------------------+
| 19200bps  TX:123  RX:120  CRC:0  TMO:2         |
+------------------------------------------------+
| [DISCOVER] [POLL ON] [PING] [READ] [INFO]      |
+------------------------------------------------+
| 0x10  Temp Node       ONLINE   2s ago          |
| 0x11  Air Node        ONLINE   1s ago          |
| 0x12  Relay Node      OFFLINE  timeout         |
+------------------------------------------------+
| BACK                                           |
+------------------------------------------------+
```

Updated target layout with pagination:

```text
+------------------------------------------------+
| SLAVE MANAGER                         RS485 OK |
+------------------------------------------------+
| 19200bps  TX:123  RX:120  CRC:0  TMO:2         |
+------------------------------------------------+
| [DISCOVER] [POLL ON] [PING] [READ] [INFO]      |
+------------------------------------------------+
| > 0x10  Temp Node       ONLINE   2s ago        |
|   MAC A1:B2:C3:D4      Temp x4, Lux optional   |
|                                                |
|   0x11  IR Node Front  ONLINE   1s ago         |
|   MAC B2:C3:D4:E5      IR Control              |
|                                                |
|   0x12  Unnamed        OFFLINE  timeout        |
|   MAC C3:D4:E5:F6      CO2                     |
+------------------------------------------------+
| BACK                         Page 1/3   NEXT > |
+------------------------------------------------+
```

Interaction rules:

```text
1. Row tap selects slave.
2. Second tap or detail icon opens Slave Detail page.
3. PING / READ / INFO operate on selected slave.
4. DISCOVER opens pairing/discovery flow.
5. NEXT cycles list page when slave count exceeds visible rows.
6. Page indicator SHALL show current page and total page count.
7. BACK returns to Settings.
```

Pagination rule:

```text
visible_rows = number of rows that fit viewport
total_pages = ceil(slave_count / visible_rows)
current_page = constrain(current_page, 0, total_pages - 1)
```

If slave count fits one page:

```text
hide NEXT
show Page 1/1 or omit page indicator
```

Visual priority:

```text
Name > Address > Status > Last seen > Capability summary
```

Low-level Modbus register values SHALL NOT be shown on this overview page.


---

# 15. Discovery Page

```text
+------------------------------------------------+
| DISCOVER SLAVE                         24s     |
+------------------------------------------------+
| Press pair button on slave module              |
|                                                |
| Candidate UID : A1B2C3D4                       |
| Capability   : TEMP | CO2 | LUX | HUMAN        |
| Suggested Addr: 0x12                           |
|                                                |
| [CANCEL]                          [ASSIGN]     |
+------------------------------------------------+
```

---

# 16. Slave Detail / Configuration Page

Saat user klik salah satu slave dari Slave Manager, HMI SHALL open Slave Detail / Configuration page.

Page ini digunakan untuk:
- lihat identitas slave
- edit nama slave
- forget/delete slave dari registry master
- lihat MAC/UID
- lihat status online/offline
- test komunikasi ke slave
- pilih sensor/control capability yang dipakai
- masuk ke dashboard mapping

Header area SHALL stay clear and readable.

Recommended layout:

```text
+------------------------------------------------+
| SLAVE DETAIL                         [BACK]    |
+------------------------------------------------+
| Name: Room Node 1                    [EDIT]    |
| MAC : A1:B2:C3:D4:E5:F6                        |
| Addr: 0x10       Status: ONLINE     Seen: 2s   |
+------------------------------------------------+
| [PING] [READ] [INFO] [SAVE]                    |
+------------------------------------------------+
| Detected / Enabled Features                    |
| [x] Temperature 1                               |
| [x] Temperature 2                               |
| [ ] Temperature 3                               |
| [ ] Temperature 4                               |
| [ ] CO2                         Unavailable    |
| [x] Lux                                        |
| [ ] Human Presence              Unavailable    |
| [ ] AC Control                  Unavailable    |
| [ ] Projector Control           Unavailable    |
| [ ] LCD Control                 Unavailable    |
| [ ] Light Relay                 Unavailable    |
|                                      scrollbar  |
+------------------------------------------------+
| [AUTO MAP]                  [MAP TO DASHBOARD] |
+------------------------------------------------+
```

Top information area rules:

```text
1. Slave name is the main identity shown to normal users.
2. MAC/UID is shown below name for technician traceability.
3. Address and online/offline status are shown in compact form.
4. Last seen should use human-readable time, e.g. "2s", "1m", "timeout".
```

Name edit behavior:

```text
IF user taps Name or EDIT
    open full-screen name editor
    show keyboard
ENDIF
```

Name editor layout:

```text
+------------------------------------------------+
| EDIT SLAVE NAME                       [CANCEL] |
+------------------------------------------------+
|                                                |
| [ Room Node 1                            ]     |
|                                                |
| On-screen keyboard                             |
|                                                |
+------------------------------------------------+
| [SAVE]                                         |
+------------------------------------------------+
```

Name storage rule:

```text
Slave name SHALL be stored in Master NVS.
Slave firmware does not need to permanently store user-facing name.
```

Feature checklist / assignment rules:

```text
1. UI SHALL show Device Profile rows first.
2. Before a profile is selected, UI MAY show all supported logical feature types as master-side assignment options.
3. Checked item means the master assigned that feature/channel to that slave.
4. Unchecked item means the feature/channel is not assigned by the master.
5. Slave-reported capability registers SHALL NOT be treated as the owner of checkbox state.
6. Master NVS is the owner of enabled/checked state.
7. SAVE SHALL write the current master assignment values to v2.1.0 Modbus capability registers `0x0010..0x0017`.
8. Profile policy SHALL be enforced by the master UI and RS485 manager, not by the slave.
9. Lux is optional when the selected profile/hardware supports Lux.
10. `IR_COMBO_NODE` SHALL allow AC 1, AC 2, and Projector together.
11. Tapping an already-selected Device Profile SHALL unselect it and return that slave to `UNASSIGNED`.
12. After a Device Profile is selected, UI SHALL hide non-profile feature rows that are outside the selected profile.
13. DELETE SHALL remove MAC/address/profile/assignment for that slave from master NVS and clear dashboard mappings that reference the slave.
```

What changed: feature selection is now profile-driven, profile rows are toggleable, irrelevant rows are hidden after profile selection, and registered slaves can be forgotten.
Why it changed: V2.1 defines master-owned Device Profiles and leaves the slave policy-blind.
Implementation effect: the checklist must enforce profile visibility/availability before SAVE so invalid assignment writes are not sent to the slave. Deleting a slave must also remove stale mapping references so the dashboard cannot keep using a forgotten UID/address.

Recommended visible feature list:

```text
Temperature 1
Temperature 2
Temperature 3
Temperature 4
CO2
Lux
Human Presence
AC Control
Projector Control
Light Relay
```

Device Profile grouping for Firmware V2.1:

- `TEMP_NODE`: Temperature 1..4, optional Lux.
- `CO2_NODE`: CO2, optional Lux.
- `PRESENCE_NODE`: Human Presence 1..4, optional Lux.
- `RELAY_NODE`: Light Relay 1..2, optional Lux.
- `IR_COMBO_NODE`: AC 1, AC 2, Projector, optional Lux.

v2.1.0 resolved items:

- Lux is defined by `LUX_SENSOR_ASSIGNMENT 0x0011` and runtime registers `0x0104..0x0107`.
- AC 1, AC 2, and Projector coexist under `IR_COMBO_NODE`.

What changed: the previous ambiguous selection details are now contract-defined.
Why it changed: v2.1.0 defines Lux and the production IR combo profile.
Implementation effect: UI can enforce Device Profile choices directly and does not need an AC/projector split decision prompt.

Temperature assignment behavior:

```text
Temperature 1 maps to dashboard Point 1 and Modbus TEMP_1_X10 (0x0100).
Temperature 2 maps to dashboard Point 2 and Modbus TEMP_2_X10 (0x0101).
Temperature 3 maps to dashboard Point 3 and Modbus TEMP_3_X10 (0x0102).
Temperature 4 maps to dashboard Point 4 and Modbus TEMP_4_X10 (0x0103).

IF Temperature N is checked on Slave A
    Temperature N SHALL show Unavailable on other slaves.
ENDIF

IF Temperature N is unchecked on Slave A
    Temperature N SHALL immediately become Available on all other slaves.
ENDIF
```

Master-to-slave SAVE behavior:

```text
On SAVE, master writes capability assignments/enables to the selected slave according to the v2.1.0 contract:

0x0010 TEMP_SENSOR_ASSIGNMENT
0x0011 LUX_SENSOR_ASSIGNMENT
0x0012 CO2_SENSOR_COUNT
0x0013 PRESENCE_SENSOR_ASSIGNMENT
0x0014 RELAY_ASSIGNMENT
0x0015 IR_PROJECTOR_ENABLE
0x0016 IR_AC_1_ENABLE
0x0017 IR_AC_2_ENABLE

SAVE SHALL write values that match the selected Device Profile. Unsupported profile rows write `0`.

Temperature assignment masks map to fixed dashboard/runtime slots:

TEMP_SENSOR_ASSIGNMENT bit 3 exposes Point 1 / TEMP_1_X10 at 0x0100
TEMP_SENSOR_ASSIGNMENT bit 2 exposes Point 2 / TEMP_2_X10 at 0x0101
TEMP_SENSOR_ASSIGNMENT bit 1 exposes Point 3 / TEMP_3_X10 at 0x0102
TEMP_SENSOR_ASSIGNMENT bit 0 exposes Point 4 / TEMP_4_X10 at 0x0103

No slave-side persistence signal is used in v2.1.0. Slave stores assignments in RAM only; master persists registry/configuration.
```

What changed: SAVE writes v2.1.0 assignment registers and no slave persistence command.
Why it changed: address and capability persistence belong to the master; the slave remains RAM-only.
Implementation effect: if the UI state violates the selected Device Profile, SAVE should reject the configuration or force the user to choose a valid profile before writing registers.

Scrollable content rule:

```text
The feature checklist SHALL be scrollable when it exceeds the available viewport.
Header, identity, and action buttons SHOULD remain visible.
```

Test action behavior:

```text
PING:
    test basic slave reachability

READ:
    read sensor/control status registers

INFO:
    read identity and capability registers
```

Test result SHALL appear as short status text, for example:

```text
Last test: PING OK 12ms
Last test: READ timeout
Last test: INFO OK Temp x4, Lux optional
```

This keeps diagnostics useful without turning the UI into a punishment device for anyone who is not a firmware engineer.


---

# 17. Dashboard Mapping Page

```text
+------------------------------------------------+
| DASHBOARD DATA MAPPING                         |
+------------------------------------------------+
| Temperature Slots                              |
| Temp 1 -> Room Node 1 / Temp 1                 |
| Temp 2 -> Room Node 1 / Temp 2                 |
| Temp 3 -> NULL                                 |
| Temp 4 -> NULL                                 |
+------------------------------------------------+
| Environment                                    |
| CO2   -> Air Node / CO2                        |
| Lux   -> Air Node / Lux                        |
| Human -> Presence Node / Human Presence        |
+------------------------------------------------+
| Control                                        |
| AC        -> IR Node Front / AC IR             |
| Projector -> IR Node Front / Projector IR      |
| Lamp      -> Relay Node / CH1                  |
+------------------------------------------------+
| [AUTO MAP] [MANUAL EDIT] [SAVE]               |
+------------------------------------------------+
```

---

# 18. Touch Scroll UI Pattern

Pages with many rows, choices, or mappings SHALL use touch scrolling instead of forcing every item into one static viewport.

This applies to:
- Slave Manager device list
- Slave Configuration capabilities
- Dashboard Mapping rows
- Manual mapping source picker
- Discovery candidate list if multiple candidates are shown
- WiFi/network list style pages

Recommended layout:

```text
+------------------------------------------------+
| PAGE TITLE                         STATUS/BACK |
+------------------------------------------------+
| Fixed action row / summary                      |
+------------------------------------------------+
|                                                |
| Scrollable content area                         |
| - row item                                     |
| - row item                                     |
| - row item                                     |
| - row item                                     |
|                                                |
|                                      scrollbar |
+------------------------------------------------+
| Optional fixed bottom actions                   |
+------------------------------------------------+
```

Touch behavior:

```text
1. Drag up/down scrolls content smoothly.
2. Scroll inertia may be used, but must stop predictably.
3. Scrollbar appears on the right side when content exceeds viewport.
4. Header and critical actions should stay visible where useful.
5. Rows must keep stable height to prevent jitter.
6. Tap targets should remain finger-friendly.
7. Text should truncate or wrap cleanly; it must not overlap.
```

Rendering recommendation:

```text
Use a clipped scroll viewport:
- draw header separately
- apply scroll offset to list rows
- skip rows outside viewport
- draw a proportional scrollbar thumb
- use velocity smoothing / low-pass movement for smooth finger scroll
```

The goal is to keep diagnostic/configuration pages dense but not semrawut.

---

# 19. Discovery Flow

```text
1. User taps DISCOVER

2. Master enters pairing mode

3. Polling paused

4. Pairing countdown starts

5. Slave listens on pairing/default Modbus address `247`

6. Master reads:
   - MAC from identity registers `0x0002..0x0004`
   - firmware version from `0x0001`
   - current node address from `0x0000`

7. If MAC is unknown, UI marks the candidate as `UNPAIRED_DEVICE_DETECTED`

8. User selects Device Profile, device name, room, and enabled channels

9. User presses ASSIGN

10. Master writes capability registers `0x0010..0x0017` according to the selected Device Profile

11. Master assigns address by writing `NODE_ADDRESS 0x0000` at address `247`

12. Master confirms the assigned address responds

13. Registry saved with MAC, address, Device Profile, device name, and room

14. Dashboard mapping recomputed

15. Polling resumed
```

Startup saved-slave flow:

1. START.
2. Master checks saved slave registry.
3. If saved slave exists, try reconnect/recovery using MAC/address mapping.
4. If no saved slave exists, do nothing automatically.
5. User starts DISCOVER only when a new slave must be assigned.

Known-device automatic recovery:

```text
1. Known assigned address does not respond.
2. Master writes recovery MAC + address to default address `247`.
3. Recovery write is `247:0x00F4 length 4`.
4. Master ignores Modbus response collision/error for this recovery write only.
5. Master confirms recovery by polling the recovered assigned address.
6. Master writes the saved Device Profile / assignment registers back to the recovered slave.
7. If the assigned address responds and assignment sync is issued, the device becomes online again with its previous role.
```

Unknown-device discovery:

```text
1. Slave appears at address `247`.
2. MAC is not in saved registry.
3. Master sets state `UNPAIRED_DEVICE_DETECTED`.
4. Master SHALL NOT auto assign address, capability, profile, name, room, or registry row.
5. User must explicitly pair the device.
```

What changed: discovery is no longer assumed on every boot.
Why it changed: saved slave mappings must survive restarts.
Implementation effect: Slave Manager should show restored known slaves when recovery succeeds, restore their saved Device Profile/assignment automatically, and only show pairing candidates when discovery is requested.

---

# 20. Runtime Data Flow

```text
Slave Sensor
    |
    v
RS485 Packet
    |
    v
RS485 Manager
    |
    v
Slave Registry Update
    |
    v
Mapping Manager
    |
    v
Dashboard Model
    |
    v
Dashboard UI
```

---

# 21. Offline Handling

```text
IF slave offline
    mapped slot becomes invalid/stale
ENDIF

IF slave online again
    mapping restored automatically
ENDIF
```

---

# 22. Persistence

Master SHALL persist:
- saved slave registry:
  - MAC
  - assigned address
  - Device Profile
  - device name
  - room
- profile-owned capability assignment state
- dashboard mapping
- manual override flags

Master SHOULD keep these as runtime state or persist them only on important events / low-frequency intervals:
- last seen
- online/offline status

Slave SHALL NOT persist address or capability configuration. After reboot, the slave returns to address `247` and waits for pairing or recovery.

---

# 23. Final Engineering Principles

```text
1. Dashboard SHALL NOT hardcode slave address.
2. Dashboard SHALL read logical slots only.
3. Auto-map SHALL fill empty slot automatically.
4. Manual override SHALL win against auto-map.
5. Offline slave SHALL invalidate slot safely.
6. Dashboard SHALL support NULL slot state.
7. IR control SHALL route through RS485 Manager.
8. Master SHALL own orchestration logic.
9. Configuration and mapping pages with many rows SHALL use touch scrolling.
10. Slave Manager SHALL support pagination or scrolling when slave count exceeds viewport.
11. Tapping a slave SHALL open a dedicated Slave Detail / Configuration page.
12. Slave name editing SHALL use a full-screen keyboard flow.
13. HMI SHALL not expose single-sensor/multi-sensor mode switch to normal users.
14. Firmware V2.1 SHALL use master-owned Device Profiles.
15. `IR_COMBO_NODE` SHALL allow AC 1, AC 2, and Projector together.
16. Lux SHALL use v2.1.0 assignment/runtime registers.
17. Capability writes SHALL use `0x0010..0x0017`.
18. Address assignment SHALL write `NODE_ADDRESS 0x0000`.
19. Known-device recovery SHALL use `247:0x00F4 length 4`, then confirm the assigned address.
20. Unknown devices SHALL enter `UNPAIRED_DEVICE_DETECTED` until user pairing.
21. Saved registry SHALL persist MAC, address, Device Profile, device name, and room.
22. Saved registry DELETE SHALL forget that slave and clear mappings referencing its UID/address.
23. Last seen/status SHALL be runtime or low-frequency persisted fields.
```

---

# END DOCUMENT
