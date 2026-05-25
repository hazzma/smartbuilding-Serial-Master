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
| TempNode |  | AirNode   |  | IRNode    |
| Temp x2  |  | CO2,Lux   |  | AC,Proj   |
|          |  | Presence  |  |           |
+----------+  +-----------+  +-----------+
```

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
    uint32_t uid;

    char name[24];

    uint16_t capability_mask;
    uint16_t enabled_mask;

    uint8_t temp_sensor_count;
    uint8_t relay_count;
    uint8_t ir_count;

    bool online;

    uint32_t last_seen;
};
```

---

# 5. Capability Model

```cpp
enum CapabilityBit {
    CAP_TEMP          = 1 << 0,
    CAP_CO2           = 1 << 1,
    CAP_HUMAN_PRESENCE= 1 << 2,
    CAP_AC_IR         = 1 << 3,
    CAP_PROJECTOR_IR  = 1 << 4,
    CAP_LIGHT_RELAY   = 1 << 5,
    CAP_LUX           = 1 << 6,
    CAP_LCD_CTRL      = 1 << 7
};
```

Slave dapat memiliki banyak capability sekaligus.

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
TEMP_SLOT_1 -> Slave A Temp Sensor 0
TEMP_SLOT_2 -> Slave A Temp Sensor 1
TEMP_SLOT_3 -> NULL
TEMP_SLOT_4 -> NULL

CO2_MAIN -> Slave B CO2
LUX_MAIN -> Slave B Lux

AC_CONTROL -> Slave C AC IR
PROJECTOR_CONTROL -> Slave C Projector IR
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

3. Isi LUX_MAIN dari slave online pertama yang memiliki lux aktif.

4. Isi HUMAN_PRESENCE_MAIN dari slave online pertama yang memiliki human presence aktif.

5. Isi AC_CONTROL dari slave online pertama yang memiliki AC IR capability aktif.

6. Isi PROJECTOR_CONTROL dari slave online pertama yang memiliki Projector IR aktif.

7. Manual override SHALL NOT ditimpa auto-map.
```

---

# 11.1 HMI Mode Selection Rule

HMI SHALL NOT require a switch between:

```text
1 slave = 1 sensor
```

and:

```text
1 slave = multiple sensors
```

Both cases are represented by the same model:

```text
capability_mask + sensor_count + channel
```

The UI SHALL adapt automatically based on detected capability and count.

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
| > 0x10  Room Node 1     ONLINE   2s ago        |
|   MAC A1:B2:C3:D4      Temp x2, CO2, Lux       |
|                                                |
|   0x11  IR Node Front  ONLINE   1s ago         |
|   MAC B2:C3:D4:E5      AC IR, Projector IR     |
|                                                |
|   0x12  Unnamed        OFFLINE  timeout        |
|   MAC C3:D4:E5:F6      Temp x1                 |
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
| [x] CO2                                        |
| [x] Lux                                        |
| [x] Human Presence                             |
| [ ] AC Control                                 |
| [ ] Projector Control                          |
| [ ] LCD Control                                |
| [ ] Light Relay                                |
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
1. UI SHALL show the supported logical feature types as master-side assignment options.
2. By default, each option SHALL be selectable/Available in every online slave detail page.
3. Checked item means the master assigned that feature/channel to that slave.
4. Unchecked item means the feature/channel is not assigned by the master.
5. Slave-reported assignment registers SHALL NOT be treated as the owner of checkbox state.
6. Master NVS is the owner of enabled/checked state.
7. SAVE SHALL write the current master assignment to the slave Modbus assignment registers.
```

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
LCD Control
Light Relay
```

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
On SAVE, master writes 0x0010..0x0017 to the selected slave.

TEMP_SENSOR_ASSIGNMENT at 0x0010:
Temp 1 -> bit3 / value 0x0008
Temp 2 -> bit2 / value 0x0004
Temp 3 -> bit1 / value 0x0002
Temp 4 -> bit0 / value 0x0001

After the assignment block write succeeds, master MAY write SAVE_CONFIG
compatibility signal 0x00F0 = 0xA55A.
```

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
Last test: INFO OK Temp x2, CO2, Lux
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

5. Slave sends PAIRING_HELLO

6. Master reads:
   - UID/MAC
   - capability
   - sensor count

7. UI shows candidate

8. User presses ASSIGN

9. Master assigns address

10. User edits name

11. User selects enabled capability

12. Registry saved

13. Dashboard mapping recomputed

14. Polling resumed
```

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
- slave registry
- slave names
- capability enable state
- dashboard mapping
- manual override flags

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
```

---

# END DOCUMENT
