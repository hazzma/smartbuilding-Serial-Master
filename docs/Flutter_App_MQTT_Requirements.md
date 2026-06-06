# Flutter App MQTT Requirements - Firmware V2

## Purpose

Dokumen ini adalah requirement MQTT untuk aplikasi Flutter yang memonitor dan mengontrol Smart Building Master pada Firmware V2.

Changed:
- Firmware V2 memakai topic MQTT per data type, bukan satu JSON besar sebagai state utama.
- Command actuator dikirim ke topic actuator, lalu master meneruskan command ke slave target.
- State actuator dipublish ulang hanya setelah target slave mengonfirmasi state terbaru.

Why:
- Per-topic MQTT membuat dashboard lebih sederhana, payload lebih kecil, dan setiap sensor/actuator bisa disinkronkan secara terpisah.

Implementation effect:
- Flutter app harus subscribe ke topic sensor/actuator yang relevan untuk class/room terpilih.
- Firmware master harus publish state per sensor type dan subscribe command actuator per actuator type.
- Single master state JSON V1 tidak lagi menjadi primary contract.

---

## 1. Firmware V2 MQTT Topic Model

Changed:
- Topic MQTT utama sekarang mengikuti format class/room + data type.
- Contoh seperti `Class HD01 co2` dan `Class LA2 suhu` adalah contoh naming, bukan hardcoded value.

Why:
- User dan dashboard perlu melihat data berdasarkan ruang/class dan jenis data secara langsung.

Implementation effect:
- App harus mengizinkan konfigurasi class/room display name.
- Jika class/room name berubah, app dan firmware harus regenerate default topic labels/templates dari nama tersebut.
- App tidak boleh menganggap semua data datang dari satu topic state master.
- Topic final tetap harus mengikuti konfigurasi broker/master/app yang disepakati proyek.

Example publish topics, not hardcoded values:
- `Class HD01 suhu`
- `Class HD01 co2`
- `Class HD01 lux`
- `Class HD01 presence`
- `Class HD01 led`

Example subscribe/control topics, not hardcoded values:
- `Class HD01 led`
- `Class HD01 ac`
- `Class HD01 projector`

Class name examples:
- If class/room name is `HD01`, default topic labels are derived as `Class HD01 <data_type>`.
- If class/room name is `LA2`, default topic labels are derived as `Class LA2 <data_type>`.
- Example: `Class LA2 co2`, `Class LA2 suhu`, `Class LA2 led`.

Topic naming rule:
- Topic SHOULD clearly identify the class/room.
- Topic SHOULD clearly identify the sensor or actuator type.
- Topic examples in this document are examples only, not final broker configuration.

Direction rule:
- Publish/state topics and subscribe/command topics MUST be distinguishable in the final MQTT configuration.
- The labels above are human-readable examples. Final broker topics may use suffixes such as `/state` and `/cmd`, separate configured topic strings, or another documented convention.
- If a development build uses the same literal topic for command and state, firmware MUST guard against self-echo and MUST NOT treat its own confirmed state publish as a new command.

---

## 2. Publish Requirements

Changed:
- Each sensor data type SHALL have its own MQTT publish topic.
- General simple sensors SHALL publish integer payloads.
- LED SHALL publish JSON because it represents 4 LED positions.
- Temperature SHALL publish JSON because it represents 4 DHT22 readings.

Why:
- Simple sensors are easier to process as integer payloads.
- Multi-position devices need structured payloads so the app can synchronize each position correctly.

Implementation effect:
- Flutter app must parse payload type by topic/data type.
- MQTT consumers must not expect one shared master state JSON as the primary data source.

Payload rules:
- CO2: integer payload, example value `720`.
- Lux: integer payload, example value `350`.
- Presence: integer payload, example values `0` or `1`.
- Simple scalar sensors: integer payload unless a later spec explicitly says otherwise.
- LED: JSON payload with ON/OFF state per LED position.
- Temperature: JSON payload with fixed slots for 4 DHT22 sensors.

MQTT delivery policy:

| MQTT message type | QoS | Retain | App behavior |
|---|---:|---|---|
| Simple sensor publish | 0 | true | App may receive last-known integer value immediately after subscribe. |
| LED state publish | 1 | true | App should treat retained LED JSON as last confirmed LED state. |
| Temperature JSON publish | 0 | true | App may receive last-known 4-slot temperature JSON immediately after subscribe. |
| Actuator command | 1 | false | App commands must not be retained; old commands must not replay after reconnect. |
| Master/device status publish | 1 | true | App may use retained status and device metadata for discovery/last-known master state. |

What changed: QoS/retain policy is explicitly defined for Firmware V2.

Why: reconnecting apps should see latest retained state, but actuator commands must not repeat accidentally.

Implementation effect: Flutter app should accept retained sensor/status messages as last-known state and should publish commands with retain disabled.

### 2.1 Temperature Publish Payload

Changed:
- Temperature is published as JSON with 4 fixed DHT22 positions.

Why:
- One temperature slave type may represent up to 4 DHT22 readings, and the app must not compact or reorder the positions.

Implementation effect:
- Flutter app must render the 4 temperature positions as fixed slots.
- Missing or unavailable readings should remain empty/stale in their original slot.

Example payload, not hardcoded. Metadata comment lines are documentation markers only and are not MQTT payload fields:

```text
// EDIT_TARGET: docs/Flutter_App_MQTT_Requirements.md section 2.1
// EDIT_PURPOSE: Document Firmware V2 temperature MQTT JSON payload
// EDIT_REASON: Temperature uses 4 fixed DHT22 positions and cannot be represented safely as one integer
{
  "sensor": "temperature",
  "unit": "celsius",
  "values": [27, 28, null, 26],
  "positions": ["DHT22_1", "DHT22_2", "DHT22_3", "DHT22_4"],
  "timestamp_ms": 1716600000
}
```

Parsing rules:
- `values[0]` maps to DHT22 position 1.
- `values[1]` maps to DHT22 position 2.
- `values[2]` maps to DHT22 position 3.
- `values[3]` maps to DHT22 position 4.
- `null` means unavailable, not installed, or stale.
- App SHALL NOT shift non-null values into earlier slots.

### 2.2 LED Publish Payload

Changed:
- LED state is published as JSON with 4 LED positions.

Why:
- LED synchronization needs ON/OFF state per physical/logical position.

Implementation effect:
- Flutter app must treat LED publish payload as source of truth after command confirmation.
- App should update local UI state from the latest LED publish message, not from optimistic command state alone.

Example payload, not hardcoded. Metadata comment lines are documentation markers only and are not MQTT payload fields:

```text
// EDIT_TARGET: docs/Flutter_App_MQTT_Requirements.md section 2.2
// EDIT_PURPOSE: Document Firmware V2 LED MQTT JSON payload
// EDIT_REASON: LED controls four positions and must synchronize confirmed ON/OFF state
{
  "actuator": "led",
  "positions": [
    { "id": 1, "state": "ON" },
    { "id": 2, "state": "OFF" },
    { "id": 3, "state": "OFF" },
    { "id": 4, "state": "ON" }
  ],
  "timestamp_ms": 1716600000
}
```

Parsing rules:
- LED IDs SHALL use positions `1..4`.
- State values SHOULD be `ON` or `OFF`.
- Missing LED positions SHALL be treated as unavailable or stale, not OFF by default.

---

## 3. Subscribe And Command Requirements

Changed:
- Firmware V2 subscribes to actuator control topics such as LED, AC, and Projector.
- Command flow is command received, forward to target slave, wait for command result, then publish updated command/status state.
- AC and Projector command payloads SHALL be JSON.

Why:
- The app/dashboard must synchronize with the master's confirmed state or command result, not only requested state.
- IR-based AC and projector controls are one-way, so command status means command execution result only, not verified physical AC/projector state.

Implementation effect:
- Flutter app may show pending state after sending a command.
- Flutter app should consider the command resolved only after the relevant topic publishes updated state or command-result status.
- Firmware must route actuator commands to the slave that owns the actuator.

Example subscribe/control topics, not hardcoded values:
- `Class HD01 led`
- `Class HD01 ac`
- `Class HD01 projector`

Direction rule:
- These are actuator command topic labels, not necessarily the same literal MQTT topics as actuator state publish topics.
- Final implementation SHOULD configure command and state topics separately, or use a clearly documented suffix convention such as `/cmd` and `/state`.
- If the same literal topic is temporarily used, the master MUST ignore confirmed state payloads in its command handler and the app MUST not treat command echoes as confirmed state.

Command flow:
1. Flutter app publishes actuator command to the selected class/room actuator topic.
2. Master receives the MQTT command.
3. Master forwards the command to the target slave device.
4. Target slave applies command and reports command status/result.
5. Master republishes the updated actuator state or command-result topic.
6. Flutter app updates UI from the republished state/result.

IR command status rule:
- AC and Projector command status SHALL be displayed as command-result status only, such as pending, success, busy, or failed.
- The app SHALL NOT present IR command status as proof that the physical AC/projector actually changed state, because IR communication is one-way.
- If the app shows requested AC/projector power, temperature, mode, or input, it SHALL label/treat it as the last command accepted by the master, not sensor-verified device state.

AC control panel rule:
- The current Flutter app requirement is one AC control panel.
- When the master exposes both AC 1 and AC 2 under an IR-capable device profile, the master SHALL mirror the same AC command to AC 1 and AC 2.
- Flutter SHALL NOT require separate AC 1 and AC 2 panels for the current implementation.

JSON command examples, not hardcoded. Metadata comment lines are documentation markers only and are not MQTT payload fields:

```text
// EDIT_TARGET: docs/Flutter_App_MQTT_Requirements.md section 3
// EDIT_PURPOSE: Document Firmware V2 AC JSON command payload
// EDIT_REASON: AC command payload is JSON so power, target temperature, and future mode fields can be extended safely
{
  "actuator": "ac",
  "power": "ON",
  "target_c": 24
}
```

```text
// EDIT_TARGET: docs/Flutter_App_MQTT_Requirements.md section 3
// EDIT_PURPOSE: Document Firmware V2 Projector JSON command payload
// EDIT_REASON: Projector command payload is JSON so power and input fields can be extended safely
{
  "actuator": "projector",
  "power": "OFF"
}
```

Open question:
- Final JSON field names for optional AC mode and projector input may be expanded later. The required decision is no longer open: AC and Projector commands use JSON.

---

## 4. Flutter App Behavior

Changed:
- Flutter app navigation for Firmware V2 should stay simple: Home, Devices, Settings.
- Home shows selected class/room dashboard.
- Devices manages discovered MQTT masters/devices and display settings.
- Settings manages broker/topic configuration and editable class/room naming.

Why:
- Firmware V2 MQTT model is per-topic and class/room oriented, so the app should prioritize room dashboard clarity over raw master JSON inspection.

Implementation effect:
- App should maintain a selected class/room context.
- App should subscribe to the configured topics for the selected class/room.
- App should not require the legacy single state JSON to render the main dashboard.

Required screens:
- Home: selected class/room dashboard with latest sensor and actuator states.
- Devices: discovered masters/devices, class/room mapping, display names, Device Profile, status, and last seen.
- Settings: broker address, credentials if needed, and topic/class configuration.

Devices / metadata requirements:
- Device Profile SHOULD appear in the device list or device detail when the master publishes saved device registry metadata.
- Device Profile values SHOULD use the master-owned v2.1 profile names or app labels derived from them: `TEMP_NODE`, `PRESENCE_NODE`, `CO2_NODE`, `RELAY_NODE`, and `IR_COMBO_NODE`.
- Each app-facing device row SHOULD include device name, room, Device Profile, online/stale/offline status, and last seen.
- Device name and room come from the master saved registry and are user-facing metadata.
- Status and last seen are runtime metadata from the master and SHOULD NOT be treated as continuously persisted slave fields.
- The app MAY show MAC/address in device detail or developer mode when published by the master, but normal app behavior should not depend on raw Modbus register addresses.
- Unknown devices reported by the master SHOULD appear as unpaired or pending user action, not as automatically usable dashboard devices.

Settings / Device Info requirements:
- Settings SHOULD provide a second page or next-page flow when needed.
- Device Info SHALL show firmware version, for example `Firmware V2`.
- Device Info SHALL show `Firmware By Hansel Kay CE LAB`.
- Device Info SHALL allow editing class/room name.
- Editing class/room name SHALL update default topic labels/templates, for example `HD01` -> `Class HD01 co2` and `LA2` -> `Class LA2 co2`.

Home SHOULD show:
- Temperature positions 1-4 from the temperature JSON topic.
- CO2 integer value.
- Lux integer value if available.
- Presence integer/binary value if available.
- LED positions 1-4 from the LED JSON topic.
- One AC control panel if configured; master mirrors to AC 1 and AC 2 when both are exposed.
- Projector command/result state if configured.

Control behavior:
- App SHALL send commands only for available actuator topics.
- App SHOULD show pending state after command publish.
- App SHALL use the next confirmed state publish or command-result publish as the synchronized UI state.
- For IR controls, synchronized UI state means the master's latest accepted command/result, not verified physical AC/projector state.

---

## 5. Error Handling And Stale Data

Changed:
- Stale state is evaluated per topic, not only per master.

Why:
- In Firmware V2 one sensor topic may keep updating while another sensor or actuator topic becomes stale.

Implementation effect:
- Flutter app should show stale status per sensor/actuator card.
- MQTT debugging should identify which topic failed or became stale.

Rules:
- If MQTT disconnects, mark all displayed values as disconnected/stale.
- If one topic misses expected updates, mark only that topic stale.
- Keep last known value visible but visually marked stale.
- Malformed payloads should be logged in developer/debug mode.
- Integer payload parse failures should not crash the app.
- JSON payload parse failures should not overwrite the last known valid state.

---

## 6. Legacy / V1 Notes

Changed:
- The old single master state JSON model is retained only as Legacy / V1 reference.

Why:
- Older firmware and earlier app experiments may still use the V1 state topic, but Firmware V2 must not depend on it as the primary model.

Implementation effect:
- Flutter app MAY support the V1 payload as a compatibility mode.
- New Firmware V2 implementation SHOULD prioritize per-topic publish/subscribe behavior.

Legacy V1 model:
- Master published one large JSON state payload to one configured state topic.
- App subscribed to a wildcard topic such as `smart-building/master/+/state`.
- App sent JSON commands to one configured command topic.
- Payload contained combined network, slave, sensor, and control state.

Legacy compatibility guidance:
- If supporting V1, keep it behind an explicit compatibility mode.
- Do not mix V1 combined state assumptions into the Firmware V2 dashboard requirements.
- Do not require `type: smart_building_master_state` for Firmware V2 per-topic sensor payloads.

---

## 7. Versioning

Changed:
- Firmware V2 versioning is documented at the topic/payload contract level.

Why:
- Per-topic payloads may evolve independently.

Implementation effect:
- JSON payloads SHOULD include version metadata when practical.
- Integer payload topics may rely on topic contract version documented in project docs.
- App should ignore unknown JSON fields.

Rules:
- JSON payloads SHOULD ignore unknown fields.
- JSON payloads MAY include `schema_version`.
- Integer topics SHALL be parsed according to the configured data type for that topic.
- Breaking payload changes must be documented before firmware/app implementation changes.
