# Flutter App MQTT Requirements

## Purpose

Dokumen ini adalah requirement untuk aplikasi Flutter yang akan mengontrol dan memonitor Smart Building Master lewat MQTT.

App Flutter SHALL:
- connect ke broker MQTT yang sama dengan master
- subscribe state dari semua master
- menampilkan daftar master yang online/terakhir terlihat
- membiarkan user memilih master mana yang ingin ditampilkan/dikontrol
- mengirim command JSON untuk AC, projector, dan lampu channel 1-4

---

## 1. MQTT Topics

Topic exact name akan mengikuti konfigurasi master dari MQTT Setup.

Recommended default:

```text
Publish dari master ke app:
smart-building/master/+/state

Command dari app ke master:
smart-building/master/+/command
```

App Flutter SHOULD support wildcard subscription untuk menemukan banyak master:

```text
smart-building/master/+/state
```

Jika master memakai custom topic dari preset, app Flutter harus bisa diset ke topic yang sama.

Current firmware development topic comes from the master's saved MQTT setup or local firmware secret file:

```text
State publish: <configured state topic>
Command subscribe: <configured command topic>
```

Because the current firmware uses one shared topic for development, Flutter SHALL filter by JSON `type`:
- consume `smart_building_master_state`
- publish `smart_building_master_command`
- ignore unknown message types

---

## 2. Master Selection

App SHALL maintain a master registry from received state payloads.

Master identity key priority:

```text
1. device_id, if present
2. device_name + master_mac
3. device_name
```

Each master card SHOULD show:
- device name
- firmware version
- MQTT online/last seen
- WiFi/LAN status
- RS485 status
- slave count
- short room/sensor summary

App SHALL allow user to select one active master. Commands SHALL target only the selected master.

Recommended stale rule:

```text
If no state payload is received for > 3 publish intervals, mark master as stale/offline.
```

---

## 3. State Payload From Master

Master publishes JSON to the configured publish topic.

Current development firmware publishes approximately every 5 seconds while MQTT is connected. If one publish fails, the next periodic state publish is attempted on the following interval.

Flutter app SHALL parse this shape:

```json
{
  "type": "smart_building_master_state",
  "schema_version": 1,
  "device_id": "optional-stable-id",
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

Parsing rules:
- Unknown fields SHALL be ignored.
- `schema_version` and `device_id` are optional in the current firmware build.
- Missing optional objects SHALL be treated as unavailable.
- Sensor value `null` means unavailable/stale.
- App SHALL not infer a sensor is valid from placeholder values such as `-100`.
- `temperature.points_c` SHALL be treated as fixed slots: index 0 = Point 1 / Temperature 1, index 1 = Point 2 / Temperature 2, index 2 = Point 3 / Temperature 3, index 3 = Point 4 / Temperature 4. Do not compact non-null values upward in the app UI.
- `lights.channels` SHALL be treated as the source of truth for lamp count and current state.
- Current agreed RS485 slave contract `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract_V_1_4_0.md` supports Relay 1-2 per slave at registers `0x0130..0x0131`. The app may still show up to 4 logical lamp channels when the master maps relays from multiple slaves or future hardware into `lights.channels`.

---

## 4. Command Payload From Flutter App

App sends JSON to the selected master's configured command topic.

Minimum command shape:

```json
{
  "type": "smart_building_master_command",
  "schema_version": 1,
  "target": "Meeting Room Master",
  "command_id": "mobile-1700000000-001",
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
- `target` SHOULD equal selected `device_name` or selected `device_id`.
- `command_id` SHOULD be unique per app action for debugging/deduplication.
- App SHALL send explicit final states, not toggle-only commands.
- App MAY send only one control object at a time.
- App SHALL only show controls where `available == true` in latest state.
- Lamp channels SHALL use `id` values `1..4`.

---

## 5. Individual Command Examples

Turn lamp 2 on:

```json
{
  "type": "smart_building_master_command",
  "schema_version": 1,
  "target": "Meeting Room Master",
  "command_id": "mobile-light2-on-001",
  "controls": {
    "lights": [
      { "id": 2, "power": true }
    ]
  }
}
```

Turn all lamps off:

```json
{
  "type": "smart_building_master_command",
  "schema_version": 1,
  "target": "Meeting Room Master",
  "command_id": "mobile-lights-off-001",
  "controls": {
    "lights": [
      { "id": 1, "power": false },
      { "id": 2, "power": false },
      { "id": 3, "power": false },
      { "id": 4, "power": false }
    ]
  }
}
```

Turn projector on:

```json
{
  "type": "smart_building_master_command",
  "schema_version": 1,
  "target": "Meeting Room Master",
  "command_id": "mobile-projector-on-001",
  "controls": {
    "projector": {
      "power": true
    }
  }
}
```

Set AC target and power:

```json
{
  "type": "smart_building_master_command",
  "schema_version": 1,
  "target": "Meeting Room Master",
  "command_id": "mobile-ac-24-001",
  "controls": {
    "ac": {
      "power": true,
      "target_c": 24
    }
  }
}
```

---

## 6. Flutter UI Requirements

App screens:

```text
1. Broker Setup / Connection
2. Master List
3. Master Dashboard
4. Master Detail / Slave List
5. Control Panel
```

Master Dashboard SHOULD show:
- average room temperature
- CO2
- human presence
- AC state and target
- projector state
- lamp 1-4 states
- slave online/offline summary

Lamp UI:
- If 1 channel: show one large ON/OFF control.
- If 2-4 channels: show individual controls for Lamp 1, Lamp 2, Lamp 3, Lamp 4.
- App SHALL NOT use one generic lamp toggle to control all lamps unless user explicitly chooses "All Lamps".

---

## 7. Error Handling

App SHALL:
- show MQTT disconnected state
- show selected master stale/offline state
- disable unavailable controls
- keep last known values visible but visually marked stale
- log or display malformed JSON errors in developer/debug mode only

Master command response is not required for first implementation. The app SHALL treat the next state publish as confirmation.

---

## 8. Versioning

Payloads SHALL include:

```text
schema_version: 1
```

Flutter app SHALL ignore unknown fields so firmware can add new sensors/controls later without breaking old app builds.
