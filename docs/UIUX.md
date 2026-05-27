# UIUX.md
# Smart Building Master S3 - Dashboard UI/UX Specification

## Document Purpose

Dokumen ini mendefinisikan rancangan UI/UX untuk HMI Smart Building Master S3.

Fokus utama UI baru:
- besar
- sederhana
- mudah dibaca oleh orang normal
- cocok untuk layar 480x320
- adaptive berdasarkan fitur/slave yang terdeteksi
- background/wallpaper first, dengan UI overlay transparan
- tetap punya logic yang jelas untuk implementasi firmware

Dokumen ini dipisahkan dari FSD agar FSD tetap menjadi spesifikasi sistem, sementara dokumen ini menjadi spesifikasi pengalaman pengguna dan aturan layout.

---

# 1. Core UI Philosophy

Dashboard utama SHALL be simple and large.

User utama tidak perlu melihat:
- detail slave
- register
- address
- mapping
- debug counter
- semua sensor sekaligus

Dashboard hanya menampilkan informasi penting yang aktif dan tersedia.

Design goal:

```text
User cukup lihat:
- jam
- suhu rata-rata
- status koneksi
- kontrol yang tersedia
- warning ringan jika ada sensor penting offline
```

Engineer/admin tetap bisa masuk ke menu detail melalui Settings.

---

# 2. Screen Resolution

Target display:

```text
480 x 320 landscape
```

Coordinate assumption:

```text
x: 0 - 479
y: 0 - 319
```

---

# 3. Wallpaper / Background Support

UI SHALL support wallpaper/background image.

Firmware SHALL provide a way to include a wallpaper asset. Current implementation uses the generated asset in:

```text
/Aset/Wallpaper.cpp
/src/wallpaper_asset.cpp
/src/wallpaper_asset.h
```

Alternative future storage may use:

```text
/assets/wallpaper_bg.h
/assets/wallpaper_bg.cpp
```

or:

```text
/data/wallpaper.raw
/data/wallpaper.rgb565
```

Recommended wallpaper format:

```text
RGB565
480x320
```

Wallpaper rules:

```text
1. Wallpaper SHALL be rendered first.
2. UI cards/widgets SHALL be drawn above wallpaper only when the screen needs a control surface.
3. Text readability SHALL remain priority.
4. If wallpaper is too bright/dark, widget cards SHALL use translucent or solid background.
5. Wallpaper SHALL NOT block UI performance.
6. Dashboard top/status area SHALL remain visually transparent over wallpaper.
7. Dashboard empty state SHALL NOT draw a large opaque card/panel.
8. Admin/setup screens MAY use translucent or solid panels for form/list readability.
```

---

# 4. Top Bar Layout

Top bar touch/status affordances SHALL always be available, but the bar itself SHALL NOT use a full-width opaque background on the dashboard.

The dashboard top area is an overlay, not a panel. Icons/text may render directly over the wallpaper. Small local chip backgrounds are allowed only when needed for readability, but the old full-width black/status strip SHALL NOT be used on the main dashboard.

Top left:

```text
Settings icon/menu affordance
HH:MM when no large centered clock is visible
```

Top right small status icons:

```text
WiFi
LAN
Slave Port / Fieldbus
```

Example:

```text
+------------------------------------------------+
| =   14:32                       WiFi LAN BUS   |
+------------------------------------------------+
```

If the dashboard is in empty/status mode and a large centered clock is shown, the small top-left clock SHALL be hidden to avoid duplicate time display.

The top-left Settings icon remains the only dashboard menu affordance. A separate bottom `MENU` button SHALL NOT be shown when the top-left Settings icon is present.

Status icon behavior:

```text
WiFi icon:
    visible if WiFi module enabled
    active if connected

LAN icon:
    visible if LAN module enabled
    active if connected

Slave/Bus icon:
    visible if fieldbus feature enabled
    active if at least one slave online
```

---

# 4.1 Top Icon Asset Requirements

Future bitmap icon assets for the dashboard top overlay SHOULD be placed under:

```text
/Aset/icons/
```

Preferred source format:

```text
PNG
transparent background
64 x 64 px source size
simple high-contrast shape
white or near-white foreground for default/topbar use
```

Required icons:

```text
settings_64.png
wifi_on_64.png
wifi_off_64.png
```

Optional future icons:

```text
lan_on_64.png
lan_off_64.png
bus_on_64.png
bus_off_64.png
co2_64.png
```

Runtime render target on 480x320 screen:

```text
Settings icon: 24 x 24 px visible icon inside ~48 x 48 px touch area
WiFi icon:     20-24 x 20-24 px visible icon inside top status area
```

If assets are converted into firmware C arrays, conversion SHOULD preserve transparency as either an explicit alpha/mask bitmap or a precomposited RGB565 variant matching the top overlay background style.

---

# 5. Small CO2 Indicator

CO2 SHALL appear as a small compact indicator on the top overlay row if valid, aligned with the Settings icon/time and WiFi/LAN/BUS indicators.

Example:

```text
CO2 720 ppm
```

Rules:

```text
IF co2_valid == true
    show small CO2 chip
ELSE
    hide CO2 chip
ENDIF
```

---

# 6. Dashboard Capability Flags

Dashboard layout is adaptive based on available capabilities.

Detected capability flags:

```cpp
bool has_temp;
bool has_ac;
bool has_projector;
bool has_led;
bool has_co2;
bool has_lux;
bool has_human_presence;
```

---

# 7. Temperature Widget

If temperature exists, dashboard SHALL show average temperature.

```text
AVG Temperature = average(valid temperature slots)
```

If user taps AVG temperature, UI SHALL open Temperature Detail screen.

Temperature point identity SHALL be stable:

```text
Point 1 = Temperature 1 = Modbus 0x0100
Point 2 = Temperature 2 = Modbus 0x0101
Point 3 = Temperature 3 = Modbus 0x0102
Point 4 = Temperature 4 = Modbus 0x0103
```

The UI SHALL NOT compact temperature values upward. If only Temperature 2 is assigned, it appears as Point 2, while Point 1 remains `-- / Not assigned`.

---

# 8. Temperature Large-Only Mode

If only temperature is available:

```text
has_temp == true
has_ac == false
has_projector == false
has_led == false
```

Layout:

```text
+------------------------------------------------+
| 14:32                              WiFi LAN BUS|
|                                                |
|                                                |
|                 27.8°C                         |
|              Average Room Temp                 |
|                                                |
|          Tap for 4-point detail                |
|                                                |
+------------------------------------------------+
```

Temperature SHALL be large and centered.

---

# 9. Temperature + Other Controls Mode

If temperature exists and at least one control exists:

```text
has_temp == true
AND
(has_ac || has_projector || has_led)
```

Then temperature widget SHALL shrink slightly and move into the adaptive widget grid.

When AVG temperature, AC, Projector, and LED are all visible, the dashboard SHOULD use a visually symmetric 2x2 widget grid:

```text
AVG Temperature    AC Target
Projector          LED
```

The four primary cards SHOULD share matching width and similar height so no control appears visually secondary.

Current target example:

```text
Settings 14:32  CO2 720ppm     WiFi LAN BUS

[ AVG Temperature ]  [ AC Target       ON ]
[ 27.8 C / Avg    ]  [ 24 C              ]

[ Projector ON/OFF ] [ Lamps 1-4        ]
```

Example:

```text
+------------------------------------------------+
| 14:32                              WiFi LAN BUS|
|                                      CO2 720ppm |
|                                                |
|  27.8°C        AC Target                       |
|  Avg Temp      24°C                            |
|  Detail        [ UP ] [ DOWN ]                 |
|                                                |
|             [ Projector ON/OFF ]               |
|                                                |
+------------------------------------------------+
```

---

# 10. AC Widget Behavior

AC widget SHALL appear only if AC capability/control endpoint is detected and mapped.

```text
IF has_ac == true
    show AC target temperature widget
ELSE
    hide AC widget
ENDIF
```

AC widget content:

Current AC card target:

```text
AC Target        [ ON / OFF ]
24 C             [         ]
[UP] [DOWN]
```

Legacy simplified shape:

```text
AC Target
24°C
[UP] [DOWN]
```

Initial simplified UI:

```text
Target temperature + UP/DOWN
```

Current target layout SHOULD stack AC label above the target value, with a large colored power badge on the right spanning the label/value area:

```text
AC Target        [ ON / OFF ]
24 C             [         ]
[UP] [DOWN]
```

Power badge color:
- ON uses green fill
- OFF uses red fill

---

# 11. Projector Widget Behavior

Projector control SHALL appear only if projector capability is detected and mapped.

```text
IF has_projector == true
    show Projector ON/OFF
ELSE
    hide Projector widget
ENDIF
```

Default placement:

```text
center bottom
```

Projector control SHALL use a large touch target and SHALL visually match the scale of other active dashboard controls.

If LED also exists:

```text
Projector moves left
LED moves right
```

---

# 12. LED/Lamp Widget Behavior

LED/Lamp control SHALL appear only if LED/Lamp capability is detected and mapped.

```text
IF has_led == true
    show LED/Lamp control
ELSE
    hide LED widget
ENDIF
```

LED/Lamp control SHALL support up to 4 lamp channels:

```text
Lamp 1
Lamp 2
Lamp 3
Lamp 4
```

If only one lamp channel is available, the widget MAY behave as a direct ON/OFF toggle.

If 2-4 lamp channels are available, the main dashboard widget SHALL make channel selection explicit before toggling a lamp. Acceptable UI patterns:
- segmented channel selector `1 2 3 4` inside the lamp widget
- tap aggregate Lamp widget to open a lamp detail sheet/page with four large toggles
- compact 2x2 mini-toggle grid when there is enough space

The UI SHALL NOT silently toggle all lamps when the user expected one selected channel.

Default placement logic:

```text
IF has_projector == false
    LED/Lamp widget centered
ELSE
    LED/Lamp widget right side
ENDIF
```

LED/Lamp control SHALL use a large touch target and SHALL visually match the scale of other active dashboard controls. Per-channel toggles SHALL still respect the minimum 48 x 48 px touch target where possible.

---

# 13. Projector + LED Layout Rule

Only Projector:

```text
Projector widget centered bottom
```

Only LED:

```text
LED/Lamp widget centered bottom
```

Projector + LED:

```text
Projector widget bottom-left
LED/Lamp widget bottom-right
```

Example:

```text
+------------------------------------------------+
|                                                |
|                                                |
|                                                |
|        [ Projector ON/OFF ] [ Lamps 1-4 ]      |
+------------------------------------------------+
```

---

# 14. No Temperature Mode

If no temperature exists:

```text
has_temp == false
```

Dashboard SHALL not reserve a large empty temperature space.

No Temp + AC + LED:

```text
AC left
LED right
```

No Temp + Only One Control:

```text
single control centered
```

Example:

```text
+------------------------------------------------+
| 14:32                              WiFi LAN BUS|
|                                                |
|                                                |
|                 [ Lamps 1-4 ]                  |
|                                                |
+------------------------------------------------+
```

---

# 15. Main Dashboard Layout Decision Tree

```text
START
  |
  v
Check has_temp
  |
  +-- YES --------------------------------+
  |                                       |
  |  Check has_ac/projector/led            |
  |       |                               |
  |       +-- NONE ---------------------> Temp Large Center Mode
  |       |
  |       +-- ANY ----------------------> Temp Compact + Dynamic Controls
  |
  +-- NO ---------------------------------+
          |
          v
       Check controls
          |
          +-- none ---------------------> Empty/Status Mode
          |
          +-- one control --------------> Single Control Center
          |
          +-- multiple controls --------> Split Control Layout
```

---

# 16. Widget Placement Algorithm

Pseudo-code:

```cpp
DashboardLayout compute_layout(DashboardModel d) {
    bool has_temp = d.avg_temp_valid;
    bool has_ac = d.ac_available;
    bool has_projector = d.projector_available;
    bool has_led = d.led_available;

    if (has_temp) {
        if (!has_ac && !has_projector && !has_led) {
            return LAYOUT_TEMP_CENTER_LARGE;
        }

        return LAYOUT_TEMP_COMPACT_WITH_CONTROLS;
    }

    int control_count = count_true(has_ac, has_projector, has_led);

    if (control_count == 0) {
        return LAYOUT_STATUS_EMPTY;
    }

    if (control_count == 1) {
        return LAYOUT_SINGLE_CONTROL_CENTER;
    }

    return LAYOUT_MULTI_CONTROL_SPLIT;
}
```

---

# 17. Dynamic Control Positioning

Pseudo-code:

```cpp
void place_controls() {
    if (has_projector && has_led) {
        projector_rect = LEFT_BOTTOM_CARD;
        led_rect = RIGHT_BOTTOM_CARD;
    }

    else if (has_projector) {
        projector_rect = CENTER_BOTTOM_CARD;
    }

    else if (has_led) {
        led_rect = CENTER_BOTTOM_CARD;
    }

    if (!has_temp && has_ac && has_led) {
        ac_rect = LEFT_CENTER_CARD;
        led_rect = RIGHT_CENTER_CARD;
    }

    else if (!has_temp && has_ac && !has_led && !has_projector) {
        ac_rect = CENTER_CARD;
    }
}
```

---

# 18. Swipe Gesture Detail Panel

Dashboard SHALL support swipe gesture to reveal secondary environmental data.

Secondary data:

```text
Lux
Human Presence
```

Recommended gesture:

```text
Swipe up reveals Environmental Detail Panel.
Swipe down hides it.
```

Environmental Detail Panel layout:

```text
+------------------------------------------------+
| Environment Detail                    [CLOSE]  |
+------------------------------------------------+
| Lux              420 lx                        |
| Human Presence   Detected                      |
+------------------------------------------------+
```

Rules:

```text
IF lux_valid == false
    display "--"
ENDIF

IF human_presence_valid == false
    display "Unknown"
ENDIF
```

---

# 19. Temperature Detail Screen

Opened by tapping AVG temperature.

```text
+------------------------------------------------+
| Temperature Detail                    [BACK]   |
+------------------------------------------------+
| Average: 27.8°C                                |
+------------------------------------------------+
| Point 1     27.2°C     Room Node A / Temp 1    |
| Point 2     28.0°C     Room Node A / Temp 2    |
| Point 3     --         Not assigned            |
| Point 4     --         Not assigned            |
+------------------------------------------------+
```

Rules:

```text
IF temp slot valid
    show value
ELSE IF mapped but stale
    show "STALE / Offline"
ELSE
    show "-- / Not assigned"
ENDIF
```

Point labels SHALL stay fixed:

```text
Point 1 shows Temperature 1 only.
Point 2 shows Temperature 2 only.
Point 3 shows Temperature 3 only.
Point 4 shows Temperature 4 only.
```

This is true even when only one or two temperature channels are assigned.

---

# 19.1 Slave Feature Assignment UI

Slave Detail SHALL treat feature rows as master-side assignment controls.

Rules:

```text
All supported feature rows are Available by default.
Checked means assigned/enabled by the master.
Unchecked means not assigned by the master.

If Temperature N is checked on one slave:
    Temperature N is Unavailable on other slaves.

If Temperature N is unchecked:
    Temperature N becomes Available on all slaves again.
```

SAVE behavior:

```text
Tap SAVE:
    write current capability counts to selected slave registers 0x0011..0x0016
    then optionally write SAVE_CONFIG 0x00F1 = 0xA55A
```

Temperature count mapping:

```text
TEMP_SENSOR_COUNT >= 1 -> Temperature 1 / 0x0100
TEMP_SENSOR_COUNT >= 2 -> Temperature 2 / 0x0101
TEMP_SENSOR_COUNT >= 3 -> Temperature 3 / 0x0102
TEMP_SENSOR_COUNT >= 4 -> Temperature 4 / 0x0103
```

---

# 20. Dashboard States

Normal:

```text
At least one useful widget exists.
```

Empty state:

```text
+------------------------------------------------+
| =                              WiFi LAN BUS    |
|                                                |
|                    14:32                       |
|                 No Device Active               |
|          Open Menu > Slave Manager             |
|                                                |
+------------------------------------------------+
```

Empty state SHALL be transparent over wallpaper:
- no opaque empty-state card
- large centered clock
- short centered status text
- centered clock and `No Device Active` SHOULD use high-contrast light text when wallpaper is dark
- helper text `Open Menu > Slave Manager` MAY use dark text if it reads better over the current wallpaper
- small top-left clock hidden while large centered clock is visible
- no bottom `MENU` button when the top-left Settings icon is visible

Current empty-state visual target:

```text
Wallpaper visible full-screen
Top-left Settings icon visible
Top-right connection indicators visible
Centered large HH:MM
Centered "No Device Active"
Centered helper text "Open Menu > Slave Manager"
No empty panel/card behind the message
```

Connection warning:

```text
Slave Offline
Check fieldbus connection
```

Warning SHALL be visible but not cover the entire screen unless critical.

---

# 21. User Interaction Summary

```text
Tap AVG temperature:
    open Temperature Detail Screen

Swipe up:
    open Environment Detail Panel

Tap AC UP:
    increase AC target temperature

Tap AC DOWN:
    decrease AC target temperature

Tap Projector:
    toggle projector desired state safely

Tap LED/Lamp aggregate:
    if one lamp channel exists:
        toggle that lamp safely
    else:
        open/select lamp channel controls

Tap Lamp 1-4:
    toggle selected lamp channel safely

Tap top-left Settings icon:
    open Settings
```

Backend command safety:

```text
User-facing toggle SHALL be translated into explicit final state command.
```

Example:

```text
User taps Lamp channel
Current desired state = OFF
Send SET_LIGHT_CHANNEL(channel_id, ON)
```

Do not send ambiguous TOGGLE commands to slave if reliability retry can duplicate commands.

---

# 22. Dashboard Data Inputs

DashboardModel SHOULD include:

```cpp
struct DashboardModel {
    bool avg_temp_valid;
    float avg_temp;

    bool temp_valid[4];
    float temp[4];

    bool co2_valid;
    int co2_ppm;

    bool lux_valid;
    float lux;

    bool human_presence_valid;
    bool human_presence;

    bool ac_available;
    int ac_target_temp;

    bool projector_available;
    bool projector_on;

    bool led_available;
    bool led_on[4];
    uint8_t led_count;

    bool wifi_connected;
    bool lan_connected;
    bool fieldbus_ok;

    char time_text[8];
};
```

---

# 23. Main Render Flow

```text
Render wallpaper/background
    |
    v
Render transparent top overlay
    |
    v
Render CO2 small chip if valid
    |
    v
Compute dashboard layout mode
    |
    v
Render temperature widget if available
    |
    v
Render AC widget if available
    |
    v
Render Projector widget if available
    |
    v
Render LED/Lamp widget if available
    |
    v
Render top-left Settings/menu touch area
```

---

# 24. Dashboard UI Block Diagram

```text
Slave Data / MQTT Data
        |
        v
Mapping Manager
        |
        v
DashboardModel
        |
        v
Layout Decision Engine
        |
        +----------------------------+
        |                            |
        v                            v
Widget Visibility              Widget Placement
        |                            |
        +-------------+--------------+
                      |
                      v
                Dashboard Render
```

---

# 25. UI Implementation Rules

```text
1. Dashboard SHALL be data-driven.
2. Dashboard SHALL not hardcode fixed widget presence.
3. Widget visibility SHALL follow capability/mapping availability.
4. Main UI SHALL remain readable from distance.
5. Debug information SHALL NOT appear on main dashboard.
6. Secondary details SHALL require tap/swipe.
7. Wallpaper SHALL not reduce readability; adjust text color per element before adding heavy panels.
8. Control widgets SHALL use large touch targets.
9. Text SHALL remain short and high contrast.
10. If a widget is unavailable, hide it rather than show disabled clutter.
11. Dashboard empty/status mode SHALL keep wallpaper visible and avoid decorative opaque containers.
12. Full-width opaque top bars SHALL be avoided on the dashboard.
```

---

# 26. Recommended Touch Areas

Minimum touch target:

```text
48 x 48 px
```

Important controls:
- AC UP/DOWN
- Projector ON/OFF
- Lamp 1-4 ON/OFF
- Menu
- AVG temperature card

These SHALL use larger target area where possible.

---

# 27. Future UI Notes

Future enhancements may include:
- theme color template
- wallpaper picker
- brightness control
- day/night mode
- multi-room page
- animated environmental panel
- icon pack for WiFi/LAN/fieldbus

---

# END DOCUMENT
