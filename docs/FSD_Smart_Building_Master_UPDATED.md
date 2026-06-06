# Functional Specification Document (FSD)

## Project: Smart Building Master S3 (Serial SPI HMI Central Unit)

---

## Document Information

| Field        | Value                              |
| ------------ | ---------------------------------- |
| Version      | 1.2                                |
| Status       | Active Development                 |
| Checkpoint   | Checkpoint 1.2 - Serial SPI + CTP  |
| Framework    | Arduino (PlatformIO)               |
| Target MCU   | ESP32-S3 N16R8                     |
| Display      | ILI9488 3.5" (480x320, Serial SPI) |
| Touch        | Capacitive Touch Controller (I2C)  |
| Connectivity | WiFi + Ethernet (W5500)            |
| Protocol     | MQTT (SSL/Non-SSL), EMQX Broker    |

---

## 1. Purpose

Firmware ini mengimplementasikan:

1. HMI berbasis TFT ILI9488 3.5" 480x320 untuk monitoring gedung.
2. Input capacitive touch via I2C.
3. Koneksi MQTT dual-interface: WiFi dan LAN via W5500.
4. Manajemen jaringan: WiFi connect/reconnect, non-blocking WiFi scan, LAN config, NTP sync.
5. Arsitektur dual-core RTOS agar rendering UI tetap responsif saat networking berjalan.

Sistem dirancang modular, dengan pemisahan tanggung jawab yang ketat antar modul.

---

## 1.1 Firmware V2 Change Summary

Firmware V2 updates the system contract around three simple ideas: saved-slave reconnect on startup, per-sensor MQTT topics, and master-owned device profile assignment.

What changed:
- Startup SHALL check saved slave configuration first. If saved slave data exists, the master SHALL try to reconnect those slaves. If no saved slave data exists, the master SHALL do nothing until the user starts discovery.
- MQTT publish SHALL be split per data type. Example topic labels such as `Class HD01 suhu` and `Class HD01 co2` are examples generated from editable class/room name, not hardcoded values.
- MQTT subscribe SHALL use actuator command topics such as LED, AC, and projector.
- LED payload SHALL be JSON because it carries ON/OFF state for 4 LED positions.
- Temperature payload SHALL be JSON because it carries 4 DHT22 readings.
- Simple/general sensor payloads SHALL use integer payloads unless their specific spec says otherwise.
- After an actuator command is confirmed by the target slave, the master SHALL publish the latest state again so Flutter/dashboard clients stay synchronized.
- Slave configuration SHALL follow the v2.1 Device Profile model. Master enforces profile policy; slave remains policy-blind.

Why it changed:
- Per-topic MQTT keeps the Flutter dashboard simple and avoids parsing one large mixed state payload for every update.
- Saved-slave reconnect avoids unnecessary reassignment on every boot.
- Device Profile selection prevents ambiguous assignments while allowing production profiles such as `IR_COMBO_NODE`.

Implementation effect:
- Firmware startup logic must separate saved-slave reconnect from manual discovery.
- MQTT manager work must route data and commands by topic type.
- RS485/slave configuration UI must select a Device Profile, then write only the v2.1 registers allowed by that master-owned profile.
- Existing V1 examples that assume one combined MQTT state JSON or one slave with multiple main sensor types are Legacy / V1 Notes.

---

## 1.2 Design Philosophy

- Rendering UI tidak boleh diblokir oleh proses networking, scan WiFi, DNS, DHCP, MQTT, atau NTP.
- Semua akses ke shared application state (`g_state`) wajib melewati `data_lock(g_state)` dan `data_unlock(g_state)`.
- TFT memakai bus SPI dedicated, sedangkan touch memakai bus I2C dedicated. Keduanya tidak berbagi pin data.
- `bus_mutex` tetap menjadi guard operasi display besar agar push frame tidak saling tindih dengan operasi hardware lain yang kelak ditambahkan.
- Data sensor yang kedaluwarsa (>10 detik) harus di-reset ke nilai NULL.

Engineering rule:

> Jika WiFi, LAN, MQTT, atau scan WiFi sedang gagal/lambat, UI harus tetap bisa render dan menerima touch.

---

## 2. System Block Diagram

```text
Smart Building Master S3

Core 0:
  Task_Net
    - WiFi Manager
    - Async WiFi Scan state machine
    - MQTT Manager
    - Time Manager

  Task_LAN
    - W5500 Ethernet link/init monitor

Core 1:
  Task_UI
    - Sprite render
    - TFT push via LovyanGFX

  Task_Touch
    - Capacitive touch polling over I2C

Shared:
  BuildingState g_state protected by data_mutex

Hardware:
  Display: ILI9488 Serial SPI, LovyanGFX, 480x320 landscape, rotation 3 for inverted mounting
  Touch: Capacitive Touch over I2C, SDA GPIO8, SCL GPIO9, CTP_RST GPIO3
  Ethernet: W5500 over dedicated SPI2_HOST
```

---

## 3. Data Flow Diagram

```text
MQTT Broker (EMQX)
       |
       | Firmware V2 per-sensor publish topics
       | examples: Class HD01 suhu / Class HD01 co2 / Class HD01 led
       v
MQTT Manager
       |
       | publish sensor payloads by topic
       | subscribe actuator command topics
       | forward confirmed actuator state back to MQTT
       v
g_state
       |
       | Task_UI polls ui_needs_update flag
       v
screens_render(g_state)
       |
       | p_engine->pushToDisplay()
       v
ILI9488 Display (480x320)
```

What changed: this data flow now describes Firmware V2 per-topic MQTT instead of the older single mixed payload example.

Why it changed: Flutter/app consumers can subscribe to only the class/room data they need.

Implementation effect: sensor publishing, actuator command subscription, and state synchronization must be handled as separate topic flows.

WiFi scan data flow:

```text
SCREEN_WIFI_CONFIG / SCREEN_WIFI_SCAN
       |
       | user taps SCAN / REFRESH
       v
wifi_manager_scan_request()
       |
       | set flags under data_lock:
       | wifi_scan_requested = true
       | wifi_scan_active = false
       | wifi_scan_done = false
       | wifi_scan_error = false
       | ui_needs_update = true
       v
Task_Net -> wifi_manager_loop()
       |
       | WiFi.scanNetworks(true, true)
       | returns immediately
       v
wifi_manager_scan_poll()
       |
       | WiFi.scanComplete()
       | WIFI_SCAN_RUNNING: keep UI alive
       | n >= 0: copy results to g_state
       | n < 0 error: set error state
       v
SCREEN_WIFI_SCAN renders current scan state
```

---

## 4. Hardware Specification

### 4.1 MCU

| Field     | Value              |
| --------- | ------------------ |
| SoC       | ESP32-S3           |
| Flash     | 16 MB (QIO)        |
| PSRAM     | 8 MB (OPI)         |
| CPU Speed | 240 MHz            |
| RTOS      | FreeRTOS (ESP-IDF) |

### 4.2 Pin Mapping Table

Pin mapping ini mengikuti hardware yang digunakan sekarang dan sudah disesuaikan dengan project `src/display.h`.

| Feature                | Function      | ESP32-S3 GPIO | Notes                         |
| :--------------------- | :------------ | :------------ | :---------------------------- |
| **TFT Serial SPI**     | CS            | 17            | ILI9488 chip select           |
|                        | RST           | 16            | ILI9488 reset                 |
|                        | DC            | 15            | Data/command                  |
|                        | MOSI / SDI    | 7             | SPI3 MOSI                     |
|                        | SCK           | 6             | SPI3 clock                    |
|                        | BL / LED      | 5             | Backlight PWM                 |
|                        | MISO / SDO    | 4             | SPI3 MISO, optional readback  |
| **Touch (I2C CTP)**    | SDA           | 8             | Capacitive touch I2C data     |
|                        | SCL           | 9             | Capacitive touch I2C clock    |
|                        | RST / CTP_RST | 3             | Touch controller reset        |
|                        | INT / CTP_INT | -             | Not connected / unused        |
| **Ethernet (W5500)**   | SCK           | 12            | Dedicated SPI2_HOST           |
|                        | MISO          | 13            | Dedicated SPI2_HOST           |
|                        | MOSI          | 11            | Dedicated SPI2_HOST           |
|                        | CS            | 10            | W5500 chip select             |
|                        | RST           | 18            | W5500 reset                   |
| **RS485 (MAX3485)**    | TX            | `TX` / TXD0   | UART0 TX to MAX3485 DI        |
|                        | RX            | `RX` / RXD0   | UART0 RX from MAX3485 RO      |
|                        | COM_SW        | 21            | MAX3485 DE + /RE direction    |

Critical:

- TFT and touch no longer share data pins.
- Do not use GPIO 3, 8, or 9 for any other runtime feature while touch is enabled.
- Do not move W5500 to the TFT SPI bus unless `bus_shared` and chip-select handling are redesigned.
- Keep TFT on SPI3_HOST and W5500 on SPI2_HOST to avoid display/network contention.
- RS485 uses board UART0 signals exposed as `TXD0/RXD0`; do not reinterpret those labels as arbitrary GPIO numbers.
- USB CDC remains the debug console, so hardware UART0 is available for RS485 field bus traffic.

### 4.3 Display Configuration

Display driver:

```cpp
LGFX_USE_V1
Panel: lgfx::Panel_ILI9488
Bus:   lgfx::Bus_SPI
Light: lgfx::Light_PWM
```

LovyanGFX display settings:

```cpp
cfg.spi_host    = SPI3_HOST;
cfg.spi_mode    = 0;
cfg.freq_write  = 60000000;
cfg.freq_read   = 20000000;
cfg.spi_3wire   = false;
cfg.use_lock    = true;
cfg.dma_channel = SPI_DMA_CH_AUTO;

cfg.panel_width  = 320;
cfg.panel_height = 480;
tft.setRotation(3); // runtime landscape 480x320, 180 deg from rotation 1
```

Display pin constants:

```cpp
#define TFT_CS    17
#define TFT_RST   16
#define TFT_DC    15
#define TFT_MOSI  7
#define TFT_SCLK  6
#define TFT_BL    5
#define TFT_MISO  4
```

Performance notes:

- `Task_UI` runs on Core 1 and `Task_Net` runs on Core 0, so WiFi scan should not directly block rendering.
- Full-screen `pushSprite(0, 0)` over SPI is the primary FPS bottleneck on ILI9488.
- TFT write clock SHALL default to 60MHz for the current PCB.
- 80MHz was tested but produced display corruption on this PCB; keep it as an experimental option only.
- If the display shows noise, tearing, or unstable colors, fallback write clock is 40MHz.
- If UI still feels framey during scroll, next optimization SHALL be dirty-rectangle/partial viewport pushing for `SCREEN_WIFI_SCAN` instead of full-screen push.

### 4.4 Touch Configuration

Touch hardware is capacitive I2C, not resistive ADC.

Touch pin constants:

```cpp
#define TOUCH_SDA 8
#define TOUCH_SCL 9
#define TOUCH_RST 3
```

Touch controller detection:

- Primary supported parser: FT6236U-compatible register layout at `0x38`.
- Diagnostic detection MAY scan common CTP addresses such as `0x38`, `0x5D`, and `0x14`.
- If a GT911-class address is detected, firmware SHALL log it clearly because GT911 requires a different coordinate parser.

Touch runtime behavior:

- `Task_Touch` polls every 20ms.
- Touch driver SHALL expose gesture event phases: `TOUCH_EVENT_DOWN`, `TOUCH_EVENT_MOVE`, and `TOUCH_EVENT_UP`.
- Normal buttons SHALL react only to `TOUCH_EVENT_DOWN`.
- Scrollable surfaces MAY consume `MOVE` and `UP` for drag gestures.
- Debounce minimum is 80ms.
- Coordinate output SHALL be transformed to landscape 480x320.

Current coordinate transform:

```cpp
TOUCH_SWAP_XY = true;
TOUCH_FLIP_X  = false;
TOUCH_FLIP_Y  = true;
TOUCH_MAX_X   = 480;
TOUCH_MAX_Y   = 320;
```

### 4.5 RS485 Configuration

RS485 is handled by a dedicated manager module and MAX3485 transceiver.

Runtime constants:

```cpp
#define RS485_UART_NUM 0
#define RS485_TX_PIN  TX
#define RS485_RX_PIN  RX
#define RS485_DIR_PIN 21
#define RS485_BAUDRATE 19200
```

Hardware behavior:

- `RS485_TX_PIN` maps to PCB label `TXD0`.
- `RS485_RX_PIN` maps to PCB label `RXD0`.
- `RS485_DIR_PIN` maps to PCB label `COM_SW` on GPIO21 and drives MAX3485 `DE + /RE`.
- Transmit mode: `DIR = HIGH`.
- Receive mode: `DIR = LOW`.
- UI must not access UART or direction control directly.

---

## 5. Software Architecture

### 5.1 File Structure

```text
Master S3/
|
|-- platformio.ini
|-- README.md
|
|-- src/
|   |-- main.cpp
|   |-- data.h / data.cpp
|   |-- display.h / display.cpp
|   |-- display_engine.h
|   |-- lgfx_adapter.h / lgfx_adapter.cpp
|   |-- touch.h / touch.cpp
|   |-- ui_screens.h / ui_screens.cpp
|   |-- ui_widgets.h / ui_widgets.cpp
|   |-- ui_keyboard.h / ui_keyboard.cpp
|   |-- wifi_manager.h / wifi_manager.cpp
|   |-- lan_manager.h / lan_manager.cpp
|   |-- mqtt_manager.h / mqtt_manager.cpp
|   |-- rs485_manager.h / rs485_manager.cpp
|   `-- time_manager.h / time_manager.cpp
|
`-- docs/
    |-- FSD_Smart_Building_Master.md
    `-- LAN/
        `-- PINOUTlan.md
```

### 5.2 RTOS Task Allocation

| Task         | Core | Priority | Stack | Responsibility                                      |
| :----------- | :--- | :------- | :---- | :-------------------------------------------------- |
| `Task_Net`   | 0    | 1        | 8 KB  | WiFi scan state machine, LAN/W5500, MQTT, NTP, timeout |
| `Task_RS485` | 0    | 1        | 4 KB  | RS485 polling, pairing, parser, retry/timeout       |
| `Task_Touch` | 1    | 1        | 4 KB  | Capacitive touch polling over I2C (20ms / 50Hz)     |
| `Task_UI`    | 1    | 4        | 16 KB | Sprite render + display push                        |

LAN/W5500, MQTT Ethernet client, DNS, and NTP SHALL be serialized inside `Task_Net`.
Do not run a separate `Task_LAN` unless all Ethernet/W5500 access is protected by a single shared mutex.
The current safe design keeps one network owner task to avoid concurrent calls into the Ethernet library.

#### 5.3 Screen State Machine

```text
SCREEN_DASHBOARD
       |
       | Hamburger Menu
       v
SCREEN_SETTINGS (2-Page Swipe)
       | (Swipe left / right to navigate pages)
       | (Taps on interactive elements)
   +----+----+----------------+
   v         v                v
SCREEN_WIFI_CONFIG   SCREEN_LAN_CONFIG   SCREEN_SLAVE_MANAGER
       |
       | SCAN
       v
SCREEN_WIFI_SCAN
       |
       | tap SSID
       v
SCREEN_WIFI_CONFIG
       |
       | tap SSID/password field
       v
SCREEN_KEYBOARD
```

### 5.4 WiFi State Machine

```text
WIFI_IDLE
   |
   | connect requested
   v
WIFI_CONNECTING ---- timeout/error ---> WIFI_RETRY
   |                                      |
   v                                      |
WIFI_CONNECTED <-------------------------+
```

WiFi connection and WiFi scan are separate state machines. A scan SHALL NOT block MQTT loop execution or UI rendering.

### 5.5 WiFi Scan State Machine

```text
SCAN_IDLE
   |
   | user request
   v
SCAN_REQUESTED
   |
   | Task_Net starts WiFi.scanNetworks(true, true)
   v
SCAN_RUNNING
   |
   | WiFi.scanComplete()
   +---- WIFI_SCAN_RUNNING ----> SCAN_RUNNING
   |
   +---- count >= 0 -----------> SCAN_DONE
   |
   +---- WIFI_SCAN_FAILED -----> SCAN_ERROR
   |
   +---- timeout 15s ----------> SCAN_ERROR

SCAN_DONE / SCAN_ERROR
   |
   | new request or screen refresh
   v
SCAN_REQUESTED
```

Expected screen behavior:

- `SCAN_IDLE`: show previous results if any; otherwise show empty state.
- `SCAN_REQUESTED`: show "Scanning..." immediately.
- `SCAN_RUNNING`: keep UI responsive and refresh indicator without blocking.
- `SCAN_DONE`: show sorted SSID list.
- `SCAN_ERROR`: show failure text and allow retry.

### 5.5.1 WiFi Scan Scroll Interaction

`SCREEN_WIFI_SCAN` SHALL support smooth vertical scrolling when scan results exceed the visible list area.

Interaction rules:

- Dragging inside the list area SHALL scroll the list vertically.
- A tap without meaningful movement SHALL select the touched SSID.
- Movement threshold SHALL be used to distinguish tap from drag.
- BACK and REFRESH buttons SHALL remain fixed and SHALL NOT scroll with the list.
- A vertical scrollbar SHALL be rendered on the right side of the list when content height exceeds viewport height.

Implementation technique:

```cpp
scroll_target -= touch_delta_y;
scroll_target = constrain(scroll_target, 0, max_scroll);

// Render-time smoothing:
scroll_y += (scroll_target - scroll_y) * 0.35f;
```

Rendering technique:

- Use a fixed list viewport: `WIFI_SCAN_LIST_TOP` to `WIFI_SCAN_LIST_BOTTOM`.
- Compute each row position as `row_y = list_top + index * row_h - scroll_y`.
- Skip rows outside the viewport instead of drawing every row.
- Render scrollbar thumb height from `viewport_h / content_h`.
- Render scrollbar thumb position from `scroll_y / max_scroll`.
- While `scroll_y` is still approaching `scroll_target`, `Task_UI` SHALL continue rendering animation frames through `screens_has_animation()`.

### 5.5.2 WiFi Scan While Connected

On the current ESP32-S3 Arduino WiFi stack, `WiFi.scanNetworks(true, true)` may return `WIFI_SCAN_FAILED` (-2) or later report `WIFI_SCAN_FAILED` from `WiFi.scanComplete()` when the STA is already associated with an access point or if the background auto-reconnect task is actively trying to connect.

Observed behavior:

- Default saved WiFi credentials can connect successfully.
- WiFi scan is reliable before association.
- WiFi scan may fail repeatedly after the device is already connected to an AP, or when the system is in a disconnected/connecting state and the auto-reconnect loop is busy trying to associate.

Required mitigation:

- Before a scan request starts, `wifi_manager.cpp` SHALL temporarily pause the STA connection and stop background reconnect attempts by calling `WiFi.setAutoReconnect(false)` and `WiFi.disconnect(false, false)`. This ensures that the Wi-Fi radio is completely idle and ready for a scan, preventing the stack from returning `WIFI_SCAN_FAILED` (-2) due to connection activity.
- The scan SHALL then run as an exclusive async scan.
- After scan completion, scan timeout, or scan start failure, `wifi_manager.cpp` SHALL re-enable auto-reconnect with `WiFi.setAutoReconnect(true)` and, if previous saved connection details exist and the power policy is ON, restore the connection by calling `WiFi.begin()`.
- During manual connect, auto-boot connect, or manual reconnect, `wifi_manager.cpp` SHALL ensure that `WiFi.setAutoReconnect(true)` is explicitly set before calling `WiFi.begin()`.
- This temporary disconnect is acceptable because scan is a user-requested configuration action; MQTT over WiFi may drop briefly and reconnect through the normal MQTT reconnect loop.
- `WiFi.scanNetworks(true, true)` success SHALL only be interpreted as `WIFI_SCAN_RUNNING`. Return value `0` SHALL NOT be treated as an async scan start success.

### 5.6 Network Priority Logic

```text
NVS: net_priority = 0 (WiFi) | 1 (LAN)
       |
  +----+----+
  v         v
WiFi ON   WiFi OFF
LAN OFF   LAN ON (W5500)
  |         |
  +----+----+
       v
MQTT connect
SSL via WiFi OR non-SSL via LAN
```

WiFi scan behavior under priority modes:

- If priority is WiFi and WiFi is disconnected, scan MAY run while WiFi STA is on.
- If priority is WiFi and WiFi is connected, scan SHALL temporarily disconnect, scan, then restore the saved WiFi connection.
- If priority is LAN and WiFi is powered off, scan request SHALL temporarily enable WiFi STA mode only for scan, then return to prior power policy after scan completes.
- Scan SHALL NOT trigger `WiFi.begin()` unless the user explicitly selects/connects to an SSID.
  Exception: after an exclusive scan temporarily paused an existing WiFi connection, firmware SHALL restore that previous saved connection automatically.

### 5.7 RS485 Slave Discovery UI Design

`SCREEN_SLAVE_MANAGER` SHALL be the first UI surface for RS485 discovery, pairing, status inspection, and quick selected-slave diagnostics.

The detailed Slave Manager layout, discovery flow, empty/default-device behavior, dashboard logical mapping, slave detail/configuration flow, and touch pagination/scroll rules SHALL be defined by:

```text
docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md
```

This FSD only owns the system-level requirements:

- Entry point SHALL remain `Settings -> Slave Manager`.
- UI SHALL route all RS485 actions through RS485 Manager callbacks/state flags.
- UI SHALL NOT access UART, Modbus parser, MAX3485 direction pin, or raw register transport directly.
- `DISCOVER` SHALL request pairing mode from RS485 Manager.
- `POLL ON/OFF` SHALL toggle normal RS485 polling through RS485 Manager.
- `PING`, `READ`, and `INFO` SHALL operate on the selected slave/device.
- Normal polling SHOULD pause while pairing is active.
- Discovery SHALL use Modbus pairing address `247`, not unsolicited binary `PAIRING_HELLO` packets.
- When no slave is detected/online, Slave Manager SHALL show one default empty device row with zero/empty identity.
- Device rows SHALL grow as slaves become detected/online and shrink as slaves are removed/offline, while keeping the single default empty row as the minimum UI state.

Any future change to Slave Manager layout, dashboard mapping behavior, slave configuration UX, or logical slot rules MUST be updated in the connectivity mapping design document first, then referenced here.

---

## 6. Shared State Requirements

### 6.1 Existing State

Current `NetworkState` already contains:

```cpp
bool wifi_connected;
bool lan_connected;
bool mqtt_ok;
int  net_priority;
char connected_wifi_ssid[32];
char conn_status[32];
char wifi_status_detail[64];
bool time_synced;
bool time_syncing;
char time_source[8];
char time_status[40];
```

### 6.2 Required WiFi Scan State

Add a compact scan result model:

```cpp
#define WIFI_SCAN_MAX_RESULTS 16

struct WiFiScanResult {
    char ssid[33];
    int32_t rssi;
    uint8_t encryption;
    uint8_t channel;
};
```

Add these fields to `NetworkState`:

```cpp
bool wifi_scan_requested;
bool wifi_scan_active;
bool wifi_scan_start_pending;
bool wifi_scan_radio_warming;
bool wifi_scan_done;
bool wifi_scan_error;
bool wifi_scan_has_results;
uint8_t wifi_scan_count;
uint32_t wifi_scan_requested_ts;
uint32_t wifi_scan_started_ts;
uint32_t wifi_scan_finished_ts;
uint8_t wifi_scan_start_attempts;
char wifi_scan_status[64];
WiFiScanResult wifi_scan_results[WIFI_SCAN_MAX_RESULTS];
```

Flag meaning:

| Flag                    | Meaning                                           |
| :---------------------- | :------------------------------------------------ |
| `wifi_scan_requested`   | UI requested scan; Task_Net has not started it    |
| `wifi_scan_start_pending` | Task_Net is preparing/retrying async scan start |
| `wifi_scan_radio_warming` | WiFi STA mode was enabled and radio warmup delay is active |
| `wifi_scan_active`      | Async scan has started and is being polled        |
| `wifi_scan_done`        | Last scan completed successfully                  |
| `wifi_scan_error`       | Last scan failed or timed out                     |
| `wifi_scan_has_results` | At least one SSID is available for rendering      |
| `wifi_scan_start_attempts` | Number of async scan start attempts for current request |

State update rules:

- UI callback may only set request flags and status strings under `data_lock`.
- `Task_Net` owns calls to `WiFi.scanNetworks()`, `WiFi.scanComplete()`, and `WiFi.scanDelete()`.
- WiFi scan start SHALL be two-stage: prepare STA radio/warmup, then start async scan.
- If `WiFi.scanNetworks(true, true)` returns a transient start failure, Task_Net SHALL retry a small fixed number of times before setting `wifi_scan_error`.
- If STA is connected before scan start, `wifi_manager.cpp` SHALL pause that connection, run the scan exclusively, and restore the saved connection afterward.
- `WiFi.scanNetworks(true, true)` SHALL be considered started only when it returns `WIFI_SCAN_RUNNING`.
- Scan result copying SHALL be done quickly, then `WiFi.scanDelete()` SHALL be called.
- `g_state.ui_needs_update` SHALL be set on scan start, scan completion, scan error, and result selection.
- No WiFi scan API call SHALL be made while holding `data_lock`.

### 6.3 Required Time Sync State

Time synchronization SHALL expose compact diagnostics in `NetworkState`:

```cpp
bool time_synced;
bool time_syncing;
char time_source[8];   // "WiFi", "LAN", or "-"
char time_status[40];
```

NTP rules:

- WiFi NTP may use Arduino `configTime()`.
- If WiFi is connected but local time is still invalid, LAN NTP SHALL be allowed as fallback.
- LAN NTP SHALL be non-blocking: send UDP request, poll response, timeout, retry.
- LAN NTP SHALL NOT use `delay()` inside `Task_Net`.
- LAN NTP, MQTT Ethernet client, DNS, and W5500 access SHALL remain serialized in `Task_Net`.

### 6.4 Required RS485 State

Current `BuildingState` SHALL include an `RS485State` block owned by `Task_RS485`.

```cpp
#define RS485_MAX_SLAVES 8

struct RS485SlaveState {
    uint8_t  address;
    uint32_t uid;
    uint8_t  role;
    uint16_t capability;
    uint32_t last_seen;
    bool     online;
    uint16_t error_count;
};

struct RS485State {
    bool initialized;
    bool bus_ok;
    bool pairing_requested;
    bool pairing_active;
    bool poll_enabled;
    uint8_t slave_count;
    uint32_t packets_tx;
    uint32_t packets_rx;
    uint32_t crc_errors;
    uint32_t timeout_errors;
    char status[64];
    RS485SlaveState slaves[RS485_MAX_SLAVES];
};
```

State ownership rules:

- `Task_RS485` owns UART, MAX3485 direction, parser, retry, and timeout handling.
- UI may read `g_state.rs485` and request high-level actions only.
- UI shall not parse RS485 frames.
- UI shall not manipulate `RS485_DIR_PIN`.
- Pairing request shall be represented as a flag or wrapper call, then executed by `Task_RS485`.

---

## 7. Functional Requirements

### 7.1 Display & UI

**DISP-001**  
System SHALL use PSRAM Sprite buffering for all main UI rendering.

**DISP-002**  
System SHALL only push frame to display when `g_state.ui_needs_update == true` or forced refresh interval expires.

**DISP-003**  
System SHALL force-refresh every 2000ms for clock and status bar updates.

**DISP-004**  
Screen transition SHALL set `ui_needs_update = true` immediately.

**DISP-005**  
All valid touch events SHALL set `ui_needs_update = true`.

**DISP-006**  
Display SHALL use LovyanGFX ILI9488 Serial SPI with runtime landscape resolution 480x320 and rotation `3` for the current inverted physical mounting.

**DISP-007**  
Dashboard and high-level configuration UI SHALL follow `docs/UIUX.md` as the detailed UI/UX source of truth. This FSD SHALL only define system-level UI requirements and integration constraints.

**DISP-008**  
Wallpaper-capable screens SHALL render the RGB565 wallpaper first, then render UI controls above it. Dashboard top-bar elements SHOULD be visually transparent over wallpaper unless a specific panel/card is required for readability.

**DISP-009**  
Normal-user screens SHALL prefer large touch targets and short readable text. Detailed engineering or configuration content SHALL remain under Settings and related admin screens.

### 7.2 Touch

**TOUCH-001**  
Touch polling SHALL occur every 20ms (50Hz) in dedicated `Task_Touch`.

**TOUCH-002**  
Touch SHALL use I2C pins SDA GPIO8 and SCL GPIO9.

**TOUCH-003**  
Touch reset SHALL use CTP_RST GPIO3.

**TOUCH-004**  
Touch coordinates SHALL be transformed to display-space 480x320 landscape and SHALL match display rotation `3`.

**TOUCH-005**  
Debounce SHALL be enforced at 80ms minimum between valid one-shot touch events.

**TOUCH-006**  
Touch detection errors SHALL be logged with `[TC]` prefix and SHALL NOT block UI rendering.

### 7.3 Network

**NET-001**  
System SHALL support dynamic priority switching between WiFi and LAN without reboot.

**NET-002**  
WiFi credentials SHALL be persisted in NVS namespace `"wifi_cfg"`. WiFi Setup SHALL load and display the last saved SSID and password whenever the screen is opened. Password SHALL remain masked unless the user explicitly enables password visibility.

**NET-003**  
Firmware V2 SHALL evaluate MQTT staleness per configured data topic. If one topic has no valid update for >10 seconds, only that topic's displayed value SHALL become stale/NULL; other topics that continue updating SHALL remain valid. If the MQTT connection itself is disconnected, all MQTT-backed values MAY be marked disconnected/stale together.

**NET-004**  
MQTT default transport SHALL use SSL port 8883 over WiFi and non-SSL port 1883 over LAN, unless overridden by saved MQTT setup.

**NET-005**  
WiFi Scan page SHALL display available SSIDs without blocking render, touch, MQTT, LAN, or NTP loops.

**NET-006**  
WiFi scan SHALL use asynchronous mode: `WiFi.scanNetworks(true, true)`.

**NET-007**  
WiFi scan polling SHALL use `WiFi.scanComplete()` from `Task_Net`.

**NET-008**  
WiFi scan SHALL timeout after 15 seconds and set `wifi_scan_error = true`.

**NET-009**  
WiFi scan results SHALL be capped to `WIFI_SCAN_MAX_RESULTS` and sorted by RSSI descending.

**NET-010**  
Selecting an SSID from `SCREEN_WIFI_SCAN` SHALL copy that SSID into the WiFi config form and return to `SCREEN_WIFI_CONFIG`.

**NET-011**  
`SCREEN_WIFI_SCAN` SHALL support drag scrolling for result lists larger than the viewport.

**NET-012**  
`SCREEN_WIFI_SCAN` SHALL render a right-side scrollbar when `content_h > viewport_h`.

**NET-013**  
WiFi scan scrolling SHALL use a target-offset plus render-offset smoothing model, not blocking delays.

**NET-014**  
Before a scan is requested, firmware SHALL temporarily disconnect the STA and disable background reconnect attempts using `WiFi.setAutoReconnect(false)` and `WiFi.disconnect(false, false)`. After the scan completes, fails, or times out, the firmware SHALL re-enable auto-reconnect using `WiFi.setAutoReconnect(true)` and restore the saved WiFi connection from NVS if the Wi-Fi power policy is ON.

**NET-015**  
Async scan start SHALL only be treated as successful when `WiFi.scanNetworks(true, true)` returns `WIFI_SCAN_RUNNING`; `0` SHALL NOT be treated as scan-running success.

**NET-016**  
MQTT broker, port, username, password, client ID, topic prefix or per-topic configuration, QoS/retain policy, TLS mode, preferred network interface, publish interval, and enabled/disabled state SHALL be configurable from Settings and persisted in ESP32 flash via NVS/Preferences.

**NET-017**  
MQTT Setup SHALL support selecting a saved preset or creating/editing a new preset. A preset SHALL contain all connection fields and topic fields needed to reconnect without recompiling firmware.

**NET-018**  
MQTT publish behavior SHALL follow Firmware V2 per-topic publishing. Each data type SHALL publish to its own configured topic. Simple/general sensor topics SHALL use integer payloads; LED SHALL use JSON for 4 LED ON/OFF states; temperature SHALL use JSON for 4 DHT22 readings.

**NET-019**  
MQTT subscribe behavior SHALL support actuator command topics for LED, AC, projector, and other mapped controls. Remote commands SHALL be forwarded to the target slave device. After the target slave confirms the new state, firmware SHALL publish/update the related state topic for Flutter/dashboard synchronization.

**NET-020**  
Device name SHALL be editable from Settings/Info and persisted in NVS/Preferences. Device name SHALL be used in MQTT payloads, client identity where appropriate, and UI/admin labels.

**NET-021**
The MQTT Setup entry SHALL open a summary screen before any field editor. The summary SHALL display broker host, port, TLS mode, username, masked password, and current MQTT connection status.

**NET-022**
The active MQTT broker host, port, TLS mode, username, and password SHALL be persisted in NVS/Preferences and SHALL be used by MQTT reconnect logic instead of remaining build-time-only values.

**NET-023**
MQTT `SAVE & RECONNECT` SHALL persist the active connection fields, disconnect the current MQTT session, and allow the normal MQTT reconnect loop to connect using the newly saved configuration.

### 7.4 RS485

**RS485-001**  
System SHALL use `rs485_manager.h/.cpp` as the only owner of RS485 UART and MAX3485 direction control.

**RS485-002**  
RS485 SHALL use Modbus RTU via the RS485 Manager as the primary transport. Legacy binary framed packets with SOF `0xAA55` are obsolete and SHALL NOT be used as the active master-slave transport.

**RS485-003**  
RS485 SHALL run in `Task_RS485` pinned to Core 0.

**RS485-004**  
RS485 SHALL use hardware UART0 signals exposed as `TXD0/RXD0` on the ESP32-S3 dev module, with USB CDC used for debug logging.

**RS485-005**  
`SCREEN_SLAVE_MANAGER` SHALL render slave registry and bus statistics from `g_state.rs485`.

**RS485-006**  
Discovery UI SHALL request pairing mode without directly touching UART or parser internals.

**RS485-007**  
Pairing mode SHALL pause normal polling to avoid bus contention.

**RS485-008**  
Slave discovery SHALL show timeout/countdown state and candidate device identity when available.

**RS485-009**
Startup SHALL check saved slave configuration before discovery. If saved slave data exists, firmware SHALL try reconnect/recovery for those slaves. If no saved slave data exists, firmware SHALL take no RS485 assignment action until the user starts discovery.

**RS485-010**
Firmware V2.1 slave assignment SHALL use master-owned Device Profiles. The master SHALL enforce profile policy and write the matching v2.1 capability registers; the slave SHALL remain policy-blind.

### 7.5 Concurrency

**CONC-001**  
All access to `g_state` SHALL use `data_lock(g_state)` and `data_unlock(g_state)`.

**CONC-002**  
Lock duration SHALL be minimized. No network API, I/O, `delay()`, or `vTaskDelay()` inside a locked block.

**CONC-003**  
Task_UI SHALL keep priority higher than network tasks.

**CONC-004**  
WiFi scan SHALL be advanced only from Task_Net, never from UI/touch handlers.

---

## 8. Rules: Do's and Don'ts

### DO

| # | Rule |
| - | ---- |
| 1 | SHALL use `vTaskDelay(pdMS_TO_TICKS(N))` in every task loop. |
| 2 | SHALL initialize `last_data_ts = millis()` on boot. |
| 3 | SHALL keep Task_UI stack at minimum 16KB. |
| 4 | SHALL set `ui_needs_update = true` on every UI state change, touch event, and WiFi scan state change. |
| 5 | SHALL use short `data_lock` blocks: read/copy, modify, unlock. |
| 6 | SHALL log `[NET] Alive` heartbeat every 5 seconds to Serial. |
| 7 | SHALL use module-prefixed logs: `[WIFI]`, `[SCAN]`, `[MQTT]`, `[LAN]`, `[TC]`, `[UI]`. |
| 8 | SHALL call `WiFi.scanDelete()` after copying completed scan results. |
| 9 | SHALL drive scroll animation from render frames and touch deltas, not from blocking delays. |
| 10 | SHALL route RS485 UI requests through RS485 Manager/state flags only. |

### DON'T

| # | Rule |
| - | ---- |
| 1 | SHALL NOT use `delay()` inside Task_UI, Task_Touch, or Task_Net loops. |
| 2 | SHALL NOT hold `data_lock` during WiFi, MQTT, LAN, I2C, SPI, Serial, or filesystem calls. |
| 3 | SHALL NOT render directly to `tft` object; always use the DisplayEngine/Sprite path. |
| 4 | SHALL NOT use GPIO 3, 8, or 9 for non-touch features while CTP is enabled. |
| 5 | SHALL NOT call blocking `WiFi.scanNetworks()` without async flag. |
| 6 | SHALL NOT call `WiFi.begin()` from the scan screen until the user explicitly chooses CONNECT. |
| 7 | SHALL NOT create additional Core 1 tasks with priority > 1. |
| 8 | SHALL NOT use fixed UP/DOWN buttons as the primary navigation for WiFi scan results when touch drag is available. |
| 9 | SHALL NOT access RS485 UART, parser, or MAX3485 direction pin from UI code. |

---

## 9. Non-Functional Requirements

| Metric                 | Target                       |
| :--------------------- | :--------------------------- |
| Boot to UI             | < 3 seconds                  |
| WiFi connect time      | < 15 seconds                 |
| WiFi scan timeout      | 15 seconds max               |
| MQTT reconnect retry   | every 5 seconds              |
| UI touch response      | < 80ms debounce              |
| UI forced refresh      | every 2000ms                 |
| Data staleness timeout | 10 seconds per MQTT data topic -> mark that topic stale/NULL |
| Serial baud rate       | 115200                       |
| Task_UI stack          | >= 16 KB                     |
| Touch poll rate        | 50 Hz (every 20ms)           |

---

## 10. Constraints

- `delay()` SHALL NOT be used for timing logic in RTOS task loops.
- WiFi reconnect and WiFi scan SHALL NOT block display or touch.
- MQTT `loop()` SHALL be called at minimum every 30ms when MQTT is enabled.
- MQTT publish/subscribe handling SHALL NOT directly block UI rendering, RS485 polling, WiFi scan, LAN DHCP/static handling, or touch processing.
- MQTT credentials, topics, presets, TLS mode, and device name SHALL be stored in ESP32 NVS/Preferences, not hardcoded as the only production path.
- All task communication SHALL go through `g_state`; no direct UI-to-network blocking call.
- TFT and W5500 SHALL stay on separate SPI hosts.
- Touch SHALL stay on I2C GPIO8/GPIO9 with reset on GPIO3.
- RS485 UART SHALL use board `TX/RX` symbols mapped to `TXD0/RXD0`, not manually guessed GPIO numbers.
- Slave Manager UI SHALL not block waiting for discovery or pairing responses.

---

## 10.1 MQTT Configuration, Identity, and Payload Contract

Current hardcoded MQTT fields SHALL be treated as development defaults only. Production MQTT behavior SHALL be driven by saved configuration.

### 10.1.0 Firmware V2 MQTT Model

Firmware V2 primary MQTT model SHALL be topic-per-data-type, not one combined master state JSON as the primary app contract.

What changed:
- The master SHALL publish each sensor/control state type to its own configured topic.
- Example publish topic labels: `Class HD01 suhu`, `Class HD01 co2`, `Class HD01 led`.
- Example subscribe topic labels: `Class HD01 led`, `Class HD01 ac`, `Class HD01 projector`.
- Simple/general sensor payloads SHALL be integer values.
- LED payload SHALL be JSON because it must synchronize 4 LED ON/OFF positions.
- Temperature payload SHALL be JSON because it must carry 4 DHT22 temperature readings.
- Commands received on actuator topics SHALL be forwarded to the target slave.
- After the slave confirms the actuator state, the master SHALL publish the updated state topic again.

Why it changed:
- Flutter/dashboard clients can subscribe to the exact data type they need.
- Actuator state becomes synchronized from confirmed slave state, not only from requested command intent.
- The topic model is easier for rooms/classes because each class can group its own sensor and actuator topics.

Implementation effect:
- MQTT setup must support topic templates or per-topic configuration for the room/class.
- MQTT manager must publish different payload types depending on the data type.
- Command handling must distinguish requested state from confirmed state.
- Publish/state topics and subscribe/command topics must be distinguishable in final configuration, even when their human-readable labels look similar.
- The old combined JSON state payload is Legacy / V1 compatibility unless a later agent explicitly keeps it as an additional diagnostic topic.

### 10.1.1 Persistent MQTT Setup

Firmware SHALL define an MQTT configuration model persisted in NVS/Preferences. Minimum fields:

```text
enabled
preset_name
broker_host
port
username
password
client_id
topic_prefix_or_class_name
publish_topics_or_template
subscribe_topics_or_template
use_tls
preferred_network = AUTO | WIFI | LAN
publish_interval_sec
retain_publish
qos
```

V2.4 implemented connection subset:

```text
NVS namespace: device_cfg
key: mqtt_server
key: mqtt_port
key: mqtt_tls
key: mqtt_user
key: mqtt_pass
```

The V2.4 MQTT Setup screen implements the connection subset above. Presets, editable topic templates, preferred network, and publish interval remain future extensions.

Rules:
- Saved config SHALL survive reboot and firmware soft reset.
- Password SHALL be editable but hidden by default in UI.
- If config is missing, firmware MAY load the current development default as a fallback preset.
- Client ID MAY be auto-generated from device name + MAC when left blank.
- Preferred network `AUTO` SHALL use the active network priority/availability logic.
- Publish rate SHALL be editable from MQTT Setup as seconds, using a numeric keyboard/input field.
- Firmware SHALL clamp publish rate to a safe range, recommended `1..3600` seconds, then convert to milliseconds internally for scheduling.
- Firmware V2 SHALL store an editable class/room name and MAY derive topic labels from it.
- If class/room name is `HD01`, derived example labels become `Class HD01 co2`, `Class HD01 suhu`, `Class HD01 led`, `Class HD01 ac`, and `Class HD01 projector`.
- If class/room name changes to `LA2`, derived example labels become `Class LA2 co2`, `Class LA2 suhu`, `Class LA2 led`, `Class LA2 ac`, and `Class LA2 projector`.
- Explicit per-topic overrides MAY exist later, but the first V2 behavior should keep the class-name-derived template as the simple default.
- Legacy single `publish_topic` and `subscribe_topic` fields MAY be kept only for migration/diagnostic compatibility.

### 10.1.1.1 MQTT QoS / Retain Policy

Firmware V2 SHALL use the following MQTT delivery policy unless a later team decision revises it:

| MQTT message type | QoS | Retain | Notes |
|---|---:|---|---|
| Simple sensor publish | 0 | true | Integer payloads such as CO2, presence, and future simple scalar sensors. |
| LED state publish | 1 | true | JSON payload containing 4 LED ON/OFF positions. |
| Temperature JSON publish | 0 | true | JSON payload containing 4 DHT22 positions. |
| Actuator command subscribe/publish from app | 1 | false | Commands must not be retained to avoid replaying old actuator actions after reconnect. |
| Master status publish | 1 | true | Online/status metadata for app/device discovery and last-known state. |

What changed: QoS and retain policy is no longer an open implementation choice.

Why it changed: sensors should be lightweight and last-known values should be visible after reconnect, while commands must not be replayed by retained MQTT messages.

Implementation effect: MQTT Setup MAY display these defaults, but firmware agents should use these policy values as the first V2 implementation baseline.

### 10.1.2 Future MQTT Presets

Future MQTT Setup MAY support:
- selecting an existing preset
- editing the active preset
- saving as a new preset
- deleting non-default presets
- testing connection without leaving the screen

Preset storage target:

```text
NVS namespace: mqtt_cfg
```

The first implementation MAY support a small fixed number of presets, for example 3-5 presets, to keep flash usage and UI complexity controlled.

### 10.1.3 Device Identity / Info

Firmware SHALL expose an Info screen under Settings.

Info screen SHALL show:
- firmware version
- device name, editable by keyboard
- class/room name, editable by keyboard
- firmware attribution text: `Firmware By Hansel Kay CE LAB`
- active network status
- MQTT status
- RS485 bus status
- master MAC address when available

Device name persistence target:

```text
NVS namespace: device_cfg
key: device_name
key: class_name
```

Class/room name SHALL drive the default MQTT topic labels. Device name SHALL be included in MQTT JSON payloads where applicable and MAY be shown in the top notification/status area where space allows.

### 10.1.4 MQTT Publish Format - Firmware V2 Primary

The master SHALL publish by data type. Topic names below are human-readable examples only, not final hardcoded values.

| Example publish/state topic label | Payload rule | Purpose |
|---|---|---|
| `Class HD01 suhu` | JSON | Four DHT22 temperature readings and optional average. |
| `Class HD01 co2` | Integer | CO2 ppm value. |
| `Class HD01 lux` | Integer | Lux value when available. |
| `Class HD01 human` | Integer | Presence state such as `0` or `1`. |
| `Class HD01 led` | JSON | Four LED ON/OFF states for synchronization. |

What changed: this replaces the previous single combined state JSON as the primary MQTT requirement.

Why it changed: each app screen or dashboard widget can subscribe only to the data it displays.

Implementation effect: firmware must publish simple sensors as integers and structured multi-position data as JSON.

Direction rule: actuator state publish topics and actuator command subscribe topics SHALL be separate configured topics or use a documented suffix convention such as `/state` and `/cmd`. If a development build temporarily uses the same literal topic, firmware SHALL ignore its own state payloads in the command handler to avoid self-echo loops.

### 10.1.4.1 Legacy / V1 Combined State JSON

The following combined JSON shape is Legacy / V1 compatibility. It MAY remain as an optional diagnostic or transition topic, but it SHALL NOT be the primary Firmware V2 app contract.

```json
{
  "type": "smart_building_master_state",
  "device_name": "Meeting Room Master",
  "firmware_version": "1.2",
  "publisher": "HK",
  "lab": "Computer Engineering Lab",
  "timestamp_ms": 1716600000,
  "network": {
    "priority": "wifi",
    "wifi_connected": true,
    "lan_connected": false,
    "mqtt_connected": true
  },
  "slaves": [
    {
      "address": 16,
      "uid": "D00D0001",
      "mac": "11:22:33:44:55:66",
      "name": "Room Sensor",
      "online": true,
      "capability": ["temp", "co2", "presence", "relay"],
      "enabled": ["temp", "co2", "relay"],
      "relay_count": 2
    }
  ],
  "data": {
    "temperature": {
      "avg_c": 27.8,
      "points_c": [27.2, 28.0, null, null]
    },
    "co2_ppm": 720,
    "lux": null,
    "human_presence": true
  },
  "controls": {
    "ac": {
      "available": true,
      "power": true,
      "target_c": 23
    },
    "projector": {
      "available": true,
      "power": false
    },
    "lights": {
      "available": true,
      "channels": [
        { "id": 1, "name": "Light 1", "power": true },
        { "id": 2, "name": "Light 2", "power": false },
        { "id": 3, "name": "Light 3", "power": false },
        { "id": 4, "name": "Light 4", "power": true }
      ]
    }
  }
}
```

Legacy/development defaults are loaded from local configuration. Public repository defaults SHALL use placeholders only; real broker credentials SHALL stay in ignored local files or persisted device settings.

```text
broker_host: <configured broker host>
secure_port: 8883
username: <configured username>
publish_topic: <configured state topic>
subscribe_topic: <configured command topic>
publish_interval_sec: 5
device_name: Meeting Room Master
firmware_version: 1.0.0
```

Legacy compatibility note: if a development build still uses the same topic for publish and subscribe, firmware SHALL ignore payloads with `type = smart_building_master_state` when they arrive through the subscribe callback.

For the legacy combined JSON only, invalid or unavailable sensor values SHALL be encoded as `null`, not fake numeric placeholders. For Firmware V2 per-topic integer payloads, invalid/stale handling SHALL be represented by topic staleness, retained-state policy, or a documented sentinel in the MQTT spec before implementation. RS485 slave contract v2.1 supports Relay 1-2 per slave at `0x010D..0x010E`; the MQTT/UI model may still expose up to 4 logical lamp channels when the master maps relays from multiple slaves or future hardware into LED positions.

Publish triggers:
- periodic publish using configured interval
- immediate publish after local control state confirmation
- immediate publish after remote command is confirmed by the target slave
- publish after slave online/offline/capability state changes

### 10.1.5 MQTT Subscribe Command Format - Firmware V2 Primary

The master SHALL subscribe to actuator command topics. Topic names below are human-readable examples only, not final hardcoded values.

| Example subscribe/command topic label | Command payload rule | Behavior |
|---|---|---|
| `Class HD01 led` | JSON command | Forward LED command to target slave, wait for confirmation, publish LED JSON state. |
| `Class HD01 ac` | JSON command | Forward AC command to target slave, wait for confirmation, publish latest AC state. |
| `Class HD01 projector` | JSON command | Forward projector command to target slave, wait for confirmation, publish latest projector state. |

What changed: command handling is topic-based and confirmation-based.

Why it changed: the dashboard must show actual confirmed actuator state, not only the requested state.

Implementation effect: MQTT command handling needs JSON parsing for LED, AC, and Projector commands, plus an RS485/control acknowledgement path before republishing synchronized state.

What changed: AC and Projector command payloads are now JSON, not an open text-vs-JSON choice.

Why it changed: JSON keeps actuator commands extensible for fields such as `power`, `target_c`, `mode`, and `input`.

Implementation effect: firmware and Flutter agents SHALL implement AC/Projector command parsing as JSON.

Temporary AC implementation note:
- The dashboard/control panel SHALL keep a single AC control surface for now.
- If the selected IR-capable slave exposes both AC 1 and AC 2 under the v2.1 slave contract, the master SHALL mirror the same AC command values to AC 1 and AC 2.
- Mirrored fields include power, set temperature, and mode.
- Separate AC 1 / AC 2 control surfaces are a future UI update and SHALL NOT be required for the current control panel.

### 10.1.5.1 Legacy / V1 Combined Command JSON

The following combined command JSON is Legacy / V1 compatibility. It MAY remain during transition, but Firmware V2 agents should not treat it as the primary app command contract.

```json
{
  "type": "smart_building_master_command",
  "target": "Meeting Room Master",
  "command_id": "optional-client-id-001",
  "controls": {
    "lights": [
      { "id": 1, "power": false },
      { "id": 4, "power": true }
    ],
    "projector": {
      "power": true
    },
    "ac": {
      "power": true,
      "target_c": 24
    }
  }
}
```

Rules:
- If `target` is present, command SHALL only apply when it matches device name or a supported wildcard.
- Unsupported fields SHALL be ignored safely.
- Light control SHALL support channels `1..4`. Commands MAY include one channel, several channels, or all channels.
- For backward compatibility during transition, a single `light.power` command MAY be interpreted as channel 1 only.
- Accepted commands SHALL NOT be considered synchronized until the target device confirms the state.
- RS485-backed controls SHALL route through RS485 Manager command/write paths.
- Firmware SHALL publish the updated per-topic state payload after confirmation.

### 10.1.6 MQTT and Device Configuration

Settings SHALL be divided into two static pages, navigable via horizontal swipe gestures (left/right).

```text
Settings Page 1:
- Network Priority toggle
- WiFi Setup
- LAN Setup
- Slave Manager
- RS485 status

Settings Page 2:
- MQTT Setup entry (opens dedicated summary screen)
- Device Info entry (opens dedicated Device Info screen)
```

Navigation rule:
- Page transitions SHALL be driven entirely by horizontal swipe gestures.
- Page dot indicators at the bottom center SHALL show the active page.

MQTT Setup screen SHALL include:
- broker/host field
- port field
- username field
- masked password field
- TLS toggle
- current MQTT connection status
- `SAVE & RECONNECT` action

V2.4 interaction rules:
- Opening MQTT Setup SHALL show the complete summary first and SHALL NOT immediately open a keyboard.
- Broker, port, username, and password editors SHALL open only after the matching field card is tapped.
- The active connection subset SHALL persist in `device_cfg`.
- `SAVE & RECONNECT` SHALL reconnect MQTT using the saved values.

Info screen SHALL include:
- editable device name
- editable class/room name
- read-only firmware version, shown as Firmware V2 or the compiled firmware version string
- read-only attribution: `Firmware By Hansel Kay CE LAB`
- read-only network/MQTT/RS485 status fields

Class topic effect:
- Editing class/room name SHALL update the default MQTT topic labels derived from that class name.
- Example: class `HD01` derives labels such as `Class HD01 co2`, `Class HD01 suhu`, `Class HD01 led`.
- Example: class `LA2` derives labels such as `Class LA2 co2`, `Class LA2 suhu`, `Class LA2 led`.
- Topic examples are labels/templates, not hardcoded broker constants; production topics may still add suffixes such as `/state` or `/cmd` if the MQTT setup requires separate state and command topics.

---

## 11. Implementation Roadmap

- [x] Migrate FSD from parallel display to current Serial SPI ILI9488 hardware.
- [x] Document capacitive touch I2C pinout and behavior.
- [x] Specify non-blocking WiFi scan flags and state machine.
- [x] Add WiFi scan fields to `NetworkState`.
- [x] Implement `wifi_manager_scan_request()`.
- [x] Implement async scan start/poll/copy/delete in `wifi_manager_loop()`.
- [x] Replace dummy `SCREEN_WIFI_SCAN` list with live `g_state.net.wifi_scan_results`.
- [x] Copy selected SSID from scan result into WiFi config form.
- [x] Add scan status text and retry behavior to UI.
- [x] Add touch event phases for scrollable UI surfaces.
- [x] Add smooth drag scroll and right-side scrollbar to `SCREEN_WIFI_SCAN`.
- [x] Add initial RS485 manager, parser, CRC, polling task, and state registry.
- [x] Document RS485 TXD0/RXD0 pin mapping and UI discovery design.
- [x] Implement `SCREEN_SLAVE_MANAGER` overview UI.
- [x] Add Slave Manager entry from hamburger Settings menu.
- [x] Add DISCOVER button to request RS485 pairing mode and pause polling.
- [x] Implement RS485 pairing candidate display from Modbus pairing address `247`.
- [x] Persist paired slave registry and dashboard mapping seed data in NVS.
- [ ] Firebase Sync - Sinkronisasi state gedung ke Firebase Realtime Database.
- [ ] Add advanced slave detail/control screens.

---

## 12. Example `platformio.ini` / Runtime Key Config

Current project uses `platformio.ini` plus pin constants in `src/display.h`.

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.psram_type = opi
board_upload.flash_size = 16MB
board_build.f_cpu = 240000000L

build_flags =
    -D CORE_DEBUG_LEVEL=3
    -D BOARD_HAS_PSRAM
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D CONFIG_SPIRAM_USE_MALLOC=1
    -D CONFIG_SPIRAM_TYPE_AUTO=1
```

Runtime pin config:

```cpp
#define TFT_CS    17
#define TFT_RST   16
#define TFT_DC    15
#define TFT_MOSI  7
#define TFT_SCLK  6
#define TFT_BL    5
#define TFT_MISO  4

#define TOUCH_SDA 8
#define TOUCH_SCL 9
#define TOUCH_RST 3

#define LAN_SCK   12
#define LAN_MISO  13
#define LAN_MOSI  11
#define LAN_CS    10
#define LAN_RST   18

#define RS485_UART_NUM 0
#define RS485_TX_PIN  TX
#define RS485_RX_PIN  RX
#define RS485_DIR_PIN 21
```

---


---

# 13. Updated RS485 Modbus Architecture & Dashboard UX

> **Normative reference for Slave/Dashboard UX:** Detailed slave orchestration, Slave Manager layout, discovery flow, empty/default-device behavior, slave detail/configuration UI, dashboard logical mapping, manual mapping, capability enable state, and touch pagination/scroll behavior SHALL be defined in `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`.
>
> **Normative reference for RS485 slave wire contract:** Modbus register map, pairing behavior, recovery behavior, invalid sensor values, and slave-side implementation rules SHALL follow `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md` v2.1.0.
>
> This FSD section is only the system-level summary for transport, task ownership, and integration boundaries. If this section conflicts with `Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md` on slave/dashboard UX or mapping behavior, the connectivity mapping design document SHALL win.

## 13.1 RS485 Transport Migration

RS485 architecture SHALL migrate from custom binary framed protocol into:

```text
Modbus RTU via DFRobot_RTU library
```

The system SHALL use Modbus RTU for:
- framing
- CRC handling
- ACK behavior
- timeout handling
- request/response transaction

Custom framed packet architecture SHALL NOT remain the primary communication layer.

---

## 13.2 RS485 Master-Orchestrated Philosophy

System SHALL follow:

```text
Slave = Dumb Capability Node
Master = Smart Orchestrator
```

Slave SHALL expose:
- capability
- sensor endpoint
- control endpoint
- identity
- raw data

Slave SHALL NOT permanently decide:
- dashboard slot
- room role
- AC purpose
- projector purpose
- dashboard behavior

Master SHALL own:
- runtime role assignment
- logical mapping
- dashboard orchestration
- visibility logic
- AVG temperature calculation
- control routing

---

## 13.3 Capability-Driven Architecture

Firmware V2 SHALL support the slave contract v2.1 Device Profile model.

Device profile is master-owned persistent metadata. Slave firmware SHALL remain policy-blind and only expose registers, execute Modbus requests, and apply values written by master.

Supported initial profiles:

```text
TEMP_NODE      = Temperature sensors + optional Lux
PRESENCE_NODE  = Presence sensors + optional Lux
CO2_NODE       = CO2 sensors + optional Lux
RELAY_NODE     = Relay outputs + optional Lux
IR_COMBO_NODE  = IR AC 1 + IR AC 2 + IR Projector + optional Lux
```

Selection behavior:
- Device Profile selection SHALL exist in Slave Manager / Slave Detail.
- Master SHALL enforce which capability registers are written for the selected profile.
- Slave SHALL NOT reject a profile combination based on application policy; profile policy belongs to master.
- Lux is a v2.1 wire-contract capability and MAY be enabled when the selected profile/hardware supports it.
- `IR_COMBO_NODE` intentionally allows AC 1, AC 2, and Projector under one slave to reduce hardware cost and installation complexity.

What changed: Firmware V2.1 replaces the previous one-main-sensor rule with master-owned Device Profile enforcement.

Why it changed: production hardware needs explicit profiles, including an intentional IR combo profile, while slaves stay simple and policy-blind.

Implementation effect: Slave Detail UI persists profile choices in master storage and writes v2.1 capability registers `0x0010..0x0017`; it does not ask the slave to decide dashboard policy.

### Legacy / V1 Notes

The following architecture is a V1 compatibility note only and SHALL NOT be used as the primary Firmware V2.1 configuration rule:

```text
1 Slave = Multiple Unrelated Capability Bundle
```

Example:

```text
Slave B:
TEMP + CO2 + HUMAN PRESENCE + IR
```

Firmware MAY keep old migration/debug handling, but user-facing V2.1 assignment SHALL follow Device Profile enforcement and the v2.1 register map.

---

## 13.4 Pairing & Discovery Philosophy

Modbus RTU does not provide native discovery.

System SHALL use:

```text
Universal Pairing Address + MAC-based Identity
```

Default pairing/recovery waiting address:

```text
247
```

Per slave contract v2.1, all slaves SHALL boot on address `247` because slave config is RAM-only. Normal assigned addresses SHALL be `2..246`; address `1` is reserved by system convention.

Master SHALL persist the MAC address to assigned-address mapping locally. On slave reboot, the master SHALL be able to restore a known slave by writing the saved MAC and address to recovery registers at address `247`.

Master SHALL also persist the selected Device Profile and assignment state for each known slave. After successful known-device recovery, master SHALL write the saved assignment registers back to the recovered slave before treating the device as fully restored for dashboard use.

Firmware V2 startup rule:
- START.
- Check saved slave registry.
- If saved slave exists, try reconnect/recovery automatically.
- Else, do nothing until user starts discovery.

Unknown-device rule:
- If a slave at address `247` has a MAC not present in the master registry, master SHALL mark it as `UNPAIRED_DEVICE_DETECTED`.
- Master SHALL NOT automatically pair, assign address, assign capability, or add the unknown device to registry.
- Unknown-device discovery and pairing SHALL be user initiated.

What changed: recovery is automatic for known devices; pairing/discovery is user initiated for unknown devices.

Why it changed: the master is the persistent source of truth for known slaves and should restore them before asking the user to discover anything.

Implementation effect: discovery at address `247` is for new/unpaired devices, while recovery writes can run automatically for known offline devices.

---

## 13.5 Pairing Flow

Pairing flow applies when the user adds a new slave or intentionally re-pairs a known slave. It is not the mandatory startup path.

```text
1. User taps DISCOVER / Pair Device

2. Polling pauses

3. Pairing countdown starts

4. Master scans Modbus address 247

5. Slave is already listening on address 247 after boot

6. Master reads identity registers 0x0000..0x0004

7. Master reads MAC and firmware metadata

8. UI shows detected device as UNPAIRED_DEVICE_DETECTED if MAC is unknown

9. User assigns address/name/room and selects Device Profile

10. Master writes v2.1 capability/config registers 0x0010..0x0017 according to selected profile

11. Master writes NODE_ADDRESS 0x0000 with a unique address 2..246

12. Slave applies address immediately and exits address 247

13. Master stores MAC/address/profile/name/room in persistent registry

14. Polling resumes
```

`SAVE_CONFIG` is not part of the v2.1 slave contract. Slave address and capability persistence belong to master; slave remains RAM-only.

What changed: pairing writes v2.1 assignment registers and address `0x0000`, then persists the selected Device Profile in master storage.

Why it changed: saved slave reconnect and user-selected profile assignment must stay consistent across boot.

Implementation effect: pairing code and UI should not automatically enable every register reported by a slave.

### 13.5.1 Recovery / Re-pairing Flow

Automatic recovery for a known offline device:

```text
1. Slave reboots and returns to address 247
2. Master has saved MAC -> assigned address in persistent registry
3. Known assigned address does not respond
4. Master writes recovery MAC/address to 247:0x00F4 length 4
5. Matching slave applies the recovered address
6. Non-matching slaves ignore the recovery command and remain on 247
7. Master ignores Modbus response collision/error for this recovery write only
8. Master confirms recovery by polling the recovered assigned address
9. Master reapplies the saved Device Profile / assignment registers `0x0010..0x0017`
```

Recovery registers:

```text
0x00F4 = RECOVERY_MAC_0_1
0x00F5 = RECOVERY_MAC_2_3
0x00F6 = RECOVERY_MAC_4_5
0x00F7 = RECOVERY_NODE_ADDRESS
```

---

## 13.6 Pairing Configuration Registers

The following v2.1 Modbus registers SHALL be reserved for runtime config/recovery:

```text
0x00F0 = CONFIG_VERSION
0x00F1 = LAST_ERROR
0x00F2 = UPTIME_LOW
0x00F3 = UPTIME_HIGH
0x00F4 = RECOVERY_MAC_0_1
0x00F5 = RECOVERY_MAC_2_3
0x00F6 = RECOVERY_MAC_4_5
0x00F7 = RECOVERY_NODE_ADDRESS
```

`SAVE_CONFIG` is removed in v2.1.

Slave firmware SHALL implement `NODE_ADDRESS`, capability assignment registers, config/recovery registers, sensor/state registers, and control registers from the v2.1 contract. Slave firmware SHALL keep address and capability in RAM only; the master SHALL own persistent registry data.

---

## 13.7 Slave Identity Model

Slave identity SHALL NOT rely only on Modbus address.

Slave SHALL expose:
- NODE_ADDRESS
- FW_VERSION
- MAC registers

because:
- address may change
- MAC identity must remain stable
- slaves return to address 247 after reboot

Identity registers:

```text
0x0000 = NODE_ADDRESS
0x0001 = FW_VERSION
0x0002 = MAC_0_1
0x0003 = MAC_2_3
0x0004 = MAC_4_5
```

---

## 13.8 Capability Registers

Slave contract v2.1 uses assignment/profile registers as the primary capability contract:

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

The Slave Detail checklist is master-owned. User checked/unchecked state and Device Profile SHALL live in master persistent storage and SHALL NOT be overwritten by a later capability read from the slave. Reading the slave capability registers is diagnostic/sync data only after the master has a local assignment.

On Slave Detail SAVE, firmware SHALL write current master profile/config data to `0x0010..0x0017`. Firmware SHALL NOT write a slave-side `SAVE_CONFIG` signal for v2.1.

Firmware V2.1 capability selection SHALL be interpreted as:
- `TEMP_NODE` writes temperature assignment bits and optional Lux assignment bits.
- `CO2_NODE` writes CO2 count and optional Lux assignment bits.
- `PRESENCE_NODE` writes presence assignment bits and optional Lux assignment bits.
- `RELAY_NODE` writes relay assignment bits and optional Lux assignment bits.
- `IR_COMBO_NODE` may enable AC 1, AC 2, Projector, and optional Lux on the same slave.

Temperature assignment SHALL map to fixed dashboard/runtime slots:

```text
TEMP_SENSOR_ASSIGNMENT bit 3 -> Temperature 1 / Point 1 -> data 0x0100
TEMP_SENSOR_ASSIGNMENT bit 2 -> Temperature 2 / Point 2 -> data 0x0101
TEMP_SENSOR_ASSIGNMENT bit 1 -> Temperature 3 / Point 3 -> data 0x0102
TEMP_SENSOR_ASSIGNMENT bit 0 -> Temperature 4 / Point 4 -> data 0x0103
```

---

## 13.8.1 RS485 Contract Block Diagram

The master SHALL treat the slave contract as four register groups: identity, capability/config, runtime data, and control.

```text
Smart Building Master S3
  |
  | Modbus RTU 19200 8N1, master request only
  v
RS485 Bus / MAX3485
  |
  +--> Slave at pairing/default address 247
  |      Identity read:       0x0000..0x0004
  |      Capability write:    0x0010..0x0017
  |      Address assignment:  0x0000
  |
  +--> Assigned slave address 2..246
         Runtime read:
           Sensor/state block 0x0100..0x010E
         Control writes:
           Relay              0x010D..0x010E
           AC 1               0x0200..0x0202
           AC 2               0x0203..0x0205
           AC status          0x0206..0x0207
           Projector          0x0210..0x0212
```

Recovery after slave reboot SHALL use the pairing/default address `247`:

```text
1. Master uses saved MAC/address from master persistent registry.
2. Master writes recovery MAC/address to 247:0x00F4 length 4.
3. Master ignores recovery write response collision/error only for this transaction.
4. Matching slave applies the address and leaves 247.
5. Master confirms by polling the recovered assigned address.
```

---

## 13.8.2 RS485 Header / Register Definitions

The following constants mirror `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md` v2.1.0 and SHALL be used when documenting or implementing master/slave communication.

```cpp
// EDIT_TARGET: docs/FSD_Smart_Building_Master_UPDATED.md section 13.8.2
// EDIT_PURPOSE: Mirror the agreed slave Modbus register constants in the FSD.
// EDIT_REASON: Firmware V2 master/slave implementation must use one shared register vocabulary.
#define SB_MODBUS_BAUDRATE      19200
#define SB_MODBUS_DEFAULT_ADDR  247
#define SB_MODBUS_MIN_ADDR      2
#define SB_MODBUS_MAX_ADDR      246

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
#define REG_PROJECTOR_POWER           0x0210
#define REG_PROJECTOR_INPUT           0x0211
#define REG_PROJECTOR_COMMAND_STATUS  0x0212
```

Sentinel values:

```text
Signed sensor read error:      -32768 (0x8000)
Signed sensor not assigned:    -32767 (0x8001)
Unsigned sensor read error:    0xFFFF
Unsigned sensor not assigned:  0xFFFE
```

---

## 13.9 Dashboard Philosophy

Dashboard SHALL NOT read slave data directly.

Dashboard SHALL read:

```text
Logical Slot
```

Examples:

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

Master SHALL map slave capability into logical slot.

`LUX_MAIN` is a master-side logical slot backed by v2.1 Lux assignment and runtime registers. The master writes `LUX_SENSOR_ASSIGNMENT 0x0011` when the selected profile/hardware supports Lux, then reads Lux runtime values from `0x0104..0x0107`.

What changed: Lux is no longer an undefined future register in the active v2.1 contract.

Why it changed: v2.1 defines Lux as an optional secondary capability for production device profiles.

Implementation effect: implementation agents may wire Lux UI and MQTT state to the v2.1 Lux registers when the selected device profile enables Lux.

---

## 13.10 Main Dashboard UX

Dashboard utama SHALL prioritize simplicity for non-technical users and SHALL follow `docs/UIUX.md` for detailed layout rules.

Primary dashboard SHALL display:
- clock, either small in the transparent top area or large in the center for empty state
- AVG temperature
- simplified room condition
- dynamic controls
- compact CO2, WiFi, LAN, and BUS status in the transparent top area
- top-left Settings icon entry to Settings

Detailed engineering/device configuration SHALL remain inside Settings.

The dashboard SHALL NOT show duplicate menu affordances. If the top-left Settings icon is visible in the top area, no bottom `MENU` button SHALL be rendered.

When the dashboard has no active device or control, the empty state SHALL remain wallpaper-friendly:
- no opaque empty-state card
- large centered clock
- short centered status text
- small top-left clock hidden while the centered clock is visible

---

## 13.11 Main Dashboard Layout

Recommended layout is defined in `docs/UIUX.md`. The compact diagram below is only a system-level summary and MUST NOT override `docs/UIUX.md`.

```text
+------------------------------------------------+
| =   14:32        CO2 720 ppm   WiFi LAN BUS    |
|                                                |
|                 27.8°C                         |
|              Average Room Temp                 |
|          Tap to view 4-point detail            |
|                                                |
|              [ Dynamic Controls ]              |
+------------------------------------------------+
```

Top dashboard status area SHALL be transparent over wallpaper. CO2, WiFi, LAN, and BUS indicators SHALL sit on the same top row as the Settings/time affordance. Individual status chips MAY keep small local backgrounds/borders if needed for readability.

Projector and LED control buttons SHALL use large touch-friendly dimensions and SHOULD visually match the scale of other primary dashboard widgets. They SHALL NOT render as noticeably small secondary buttons when they are active dashboard controls.

Light/LED dashboard control SHALL support up to 4 controllable lamp channels. If more than one lamp channel is available, the dashboard SHALL provide a compact channel selector or expanded lamp detail surface so the user can choose which lamp to toggle. The main dashboard MAY show an aggregate light widget, but the control interaction SHALL make channel 1-4 selection explicit before changing a specific lamp.

Empty/status layout:

```text
+------------------------------------------------+
| =                              WiFi LAN BUS    |
|                                                |
|                    14:32                       |
|              No Device Active                  |
|          Open Menu > Slave Manager             |
|                                                |
+------------------------------------------------+
```

The bottom menu button is intentionally omitted because Settings is available through the top-left Settings affordance.

---

## 13.11.1 Settings and Admin Screen UX

Settings, WiFi Setup, WiFi Scan, LAN Setup, and Slave Manager SHALL use the same large-touch, wallpaper-friendly design language defined in `docs/UIUX.md`.

System-level requirements:
- Settings SHALL be the central admin entry point for WiFi, LAN, Slave Manager, network priority, and Page 2 configuration.
- Settings SHALL support 2-page horizontal swipe navigation.
- WiFi Setup SHALL expose SSID, password visibility, reconnect, scan, and connect actions using large touch targets.
- WiFi Setup SHALL populate SSID and password from the last saved `wifi_cfg` credentials whenever the screen is opened.
- WiFi Scan SHALL present selectable SSID rows with touch scrolling; scan logic remains asynchronous as specified in section 5.5.
- LAN Setup SHALL expose DHCP/STATIC mode, current link/status, editable static IPv4 fields, and save action using large touch targets.
- Slave Manager SHALL remain the entry point for discovery, pairing, polling, diagnostics, and detail/mapping navigation.
- Settings Page 2 SHALL open dedicated MQTT Setup and Device Info screens. MQTT connection fields and device identity fields SHALL be saved dynamically to NVS/Preferences.
- Editing class/room name in Settings Page 2 SHALL update default MQTT topic labels generated by the class template, for example `Class HD01 co2` or `Class LA2 co2`.

Detailed visual layout for these surfaces SHOULD be kept in `docs/UIUX.md` or the connectivity mapping design document where applicable; this FSD SHALL avoid duplicating full screen mockups unless required for system behavior.

---

## 13.12 AVG Temperature Rule

Dashboard utama SHALL display only:

```text
AVG TEMPERATURE
```

Detailed multi-point temperature SHALL appear only after user taps AVG temperature card.

AVG SHALL be calculated from all valid mapped temperature slot.

Temperature point numbering SHALL be stable and SHALL NOT compact:

```text
Point 1 always represents Temperature 1 / 0x0100.
Point 2 always represents Temperature 2 / 0x0101.
Point 3 always represents Temperature 3 / 0x0102.
Point 4 always represents Temperature 4 / 0x0103.
```

---

## 13.13 Temperature Detail Screen

Example:

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

Offline or invalid sensor SHALL display:
```text
STALE / Offline
```

Unused slot SHALL display:
```text
Not assigned
```

---

## 13.14 Dynamic Control Visibility

Dashboard control widgets SHALL appear dynamically.

Rules:

```text
IF AC capability exists
    show AC control
ENDIF

IF projector capability exists
    show projector control
ENDIF

IF light relay capability exists
    show light control with channel selection for 1-4 available lamps
ENDIF

IF LCD capability exists
    show LCD control
ENDIF
```

Temporary AC panel rule:
- The dashboard SHALL show only one AC control widget for the current implementation.
- When an `IR_COMBO_NODE` or equivalent IR-capable device exposes AC 1 and AC 2, the single AC widget SHALL send the same power/set-temperature/mode command to both AC channels.
- Per-AC control widgets may be added later, but they are not part of the current required panel behavior.
- Expanded AC layouts SHALL enlarge the `UP` and `DOWN` touch targets while keeping the ON/OFF power badge compact.
- Expanded AC layouts SHALL show Swing and Fan cycling controls in a bottom row.
- Swing and Fan are temporary UI-only state cycles until the RS485 slave contract defines their registers; they SHALL NOT be presented as confirmed slave state.

Special adaptive layout rules:
- With AC + Projector and no temperature/LED, AC SHALL use a tall left card and Projector SHALL use a large square control centered on the right.
- With Temperature + AC + Projector and no LED, AC SHALL use the left card, Projector SHALL use a square upper-right card, and Average Temperature SHALL appear below Projector.

If capability does not exist:
- widget SHALL be hidden completely
- dashboard SHALL remain visually clean

---

## 13.15 Slave Manager UX Revision

Detailed Slave Manager layout, pagination, default empty-device row, row visual priority, row tap behavior, and detail-page navigation SHALL follow `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`.

`SCREEN_SLAVE_MANAGER` SHALL become:
- pairing center
- discovery page
- slave health monitor
- mapping entry point

It SHALL also keep quick diagnostic buttons for selected slave validation:

```text
PING = verify Modbus response at NODE_ADDRESS register
READ = read current sensor snapshot
INFO = refresh identity and capability metadata
```

These actions SHALL be routed through RS485 Manager and SHALL NOT expose raw UART/parser access to UI code.

NOT:
- raw UART terminal
- raw register editor
- low-level Modbus debug console

---

## 13.16 Slave Manager Layout

The following layout is only a compact example. The authoritative target layout, including pagination and default empty-device behavior, SHALL be maintained in `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`.

```text
+------------------------------------------------+
| Slave Manager                         RS485 OK |
+------------------------------------------------+
| Bus: Modbus RTU 19200   Online: 2/3            |
+------------------------------------------------+
| [ Discover New Slave ] [PING] [READ] [INFO]    |
+------------------------------------------------+
| Temp Node A          ONLINE                    |
| MAC A1:B2:C3:D4      Temperature x2 + Lux opt  |
|                                                |
| IR Controller        ONLINE                    |
| MAC B2:C3:D4:E5      IR Control                |
|                                                |
| Unnamed Device       NEW                       |
| MAC C3:D4:E5:F6      Temp x1                   |
+------------------------------------------------+
```

What changed: the example list no longer presents `Temp x2, CO2` as one active slave role.

Why it changed: Firmware V2 requires one main function type per slave, with Lux as the only optional secondary feature.

Implementation effect: Slave Manager summaries should show one selected main role plus optional Lux, not a V1-style mixed capability bundle.

Minimum empty state:

```text
Device 0  0x00      EMPTY
MAC --             No slave detected
```

---

## 13.17 Discovery UX

Detailed discovery UI and candidate presentation SHALL follow `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`.

Discovery UI SHALL use human-readable instruction.

Recommended flow:

```text
+------------------------------------------------+
| Discover New Slave                     24s     |
+------------------------------------------------+
| 1. Press pairing button on the slave module.   |
| 2. Keep only one new slave in pairing mode.    |
|                                                |
| Found: A1:B2:C3:D4:E5:F6                       |
| Available: Temperature, CO2, Presence          |
| Selected: Temperature x2 + Lux optional         |
|                                                |
| Suggested name: Room Sensor                    |
| Suggested address: 12                          |
|                                                |
| [Cancel]                          [Continue]   |
+------------------------------------------------+
```

UI SHALL avoid exposing raw register terminology to normal users.

What changed: discovery copy separates available choices from the selected V2 role.

Why it changed: the slave contract exposes multiple assignment registers, while Firmware V2.1 master policy chooses a Device Profile before writing them.

Implementation effect: discovery can display supported choices, but Continue/SAVE must require a selected Device Profile before writing capability registers.

---

## 13.18 Slave Configuration UI

Detailed Slave Detail / Configuration UI, name editing, detected/enabled feature checklist, and scroll behavior SHALL follow `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`.

Assignment rules:

```text
Supported profile rows SHALL default to Available before selection.
Selecting a Device Profile SHALL determine which capability rows are enabled for that slave.
Selecting an already-selected Device Profile SHALL clear the selected profile and return the slave configuration to `UNASSIGNED`.
After a Device Profile is selected, non-profile feature rows outside the selected profile SHALL be hidden from the normal configuration list rather than shown as clutter.
IR_COMBO_NODE SHALL allow AC 1, AC 2, and Projector together.
Lux SHALL remain an optional auxiliary sensor when the selected profile/hardware supports Lux.
Checked state is master-owned and persisted in master NVS.
SAVE SHALL write the selected Firmware V2.1 profile/configuration to the selected slave using the agreed slave contract registers.
DELETE SHALL remove the slave from the master saved registry and clear dashboard mappings that reference the slave UID/address. A deleted slave SHALL NOT auto-recover again until it is paired again.
```

What changed: Firmware V2.1 uses Device Profile enforcement instead of a generic one-main-sensor rule. Selected profiles now filter the feature list, profile rows are toggleable back to `UNASSIGNED`, and saved slaves can be forgotten from the registry. Dashboard temperature slots may still remain globally unique, but that is a mapping constraint, not slave policy.

Why it changed: master-owned profiles can prevent invalid combinations while intentionally allowing production profiles such as `IR_COMBO_NODE`.

Implementation effect: the Slave Detail screen needs profile-driven row visibility, enabled/unavailable states, delete/forget handling, and mapping screens may separately prevent two sources from owning the same dashboard slot.

Recommended configuration screen:

```text
+------------------------------------------------+
| Configure Slave                                |
+------------------------------------------------+
| Name: [ Room Sensor A              Edit ]      |
|                                                |
| Use this device for:                           |
| [x] Temperature                                |
| [ ] CO2                         Unavailable   |
| [ ] Presence                    Unavailable   |
| [x] Lux                         Optional      |
|                                                |
| [Save]                         [Advanced]      |
+------------------------------------------------+
```

---

## 13.19 Mapping Philosophy

Automatic mapping SHALL be the default behavior.

Manual mapping SHALL exist as:
```text
Advanced configuration
```

not primary onboarding flow.

---

## 13.20 Advanced Mapping Screen

Detailed dashboard mapping rows, source picker behavior, auto-map/manual-edit actions, and persistence rules SHALL follow `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`.

Recommended layout:

```text
+------------------------------------------------+
| Dashboard Mapping                     [BACK]   |
+------------------------------------------------+
| Temperature                                   |
| Temp 1 -> Room Sensor A / Temp 1              |
| Temp 2 -> Room Sensor A / Temp 2              |
| Temp 3 -> Not assigned                        |
| Temp 4 -> Not assigned                        |
|                                                |
| Controls                                      |
| AC        -> IR Controller / AC 1+2 mirrored  |
| Projector -> IR Controller / Projector IR     |
| LCD       -> Not available                    |
|                                                |
| [Auto Map] [Manual Edit] [Save]               |
+------------------------------------------------+
```

---

## 13.21 Slave Reset Behavior

After reset:

```text
Slave SHALL become runtime-unconfigured node.
```

Slave SHALL:
- expose capability
- expose sensor register
- wait for master synchronization

Master SHALL remain source of truth.

Firmware V2 startup behavior:
- Master SHALL first check saved slave data.
- If saved slave data exists, master SHALL try reconnect/recovery.
- If recovery succeeds, master SHALL reapply the saved Device Profile and assignment state to the slave.
- If no saved slave data exists, master SHALL not auto-assign or auto-discover slaves.

What changed: reset/startup recovery is saved-state-driven instead of always starting from fresh pairing.

Why it changed: saved slaves should keep stable identity and mapping across firmware restarts.

Implementation effect: firmware startup, RS485 recovery, and UI empty-state handling must be coordinated so no phantom slave is assigned automatically.

---

## 13.22 Final Engineering Principles

1. Dashboard SHALL NOT hardcode slave address.
2. Dashboard SHALL use logical slot abstraction.
3. Slave SHALL expose capability, not permanent role.
4. Master SHALL assign runtime behavior.
5. Single-sensor slave SHALL be supported.
6. Firmware V2.1 slave assignment SHALL use master-owned Device Profiles.
7. Slave SHALL remain policy-blind; master owns profile, room, naming, and mapping.
8. Dynamic control visibility SHALL be supported.
9. AVG temperature SHALL be dashboard primary metric.
10. Detailed temperature SHALL exist on separate detail page.
11. Modbus RTU SHALL remain the primary RS485 transport layer.
12. Saved-slave reconnect SHALL run before manual discovery.
13. MQTT SHALL use per-sensor publish topics and actuator subscribe topics as the Firmware V2 primary model.
14. Slave Manager, Discovery, Slave Detail, and Dashboard Mapping UX SHALL reference `Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md` as the detailed source of truth.
