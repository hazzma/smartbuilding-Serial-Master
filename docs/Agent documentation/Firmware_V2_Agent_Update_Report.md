# Firmware V2 Agent Update Report

## Purpose

This report records the documentation-only agent pass for Smart Building Firmware V2.

What changed:
- Documentation was aligned around Firmware V2 startup, MQTT, slave assignment, and app/dashboard behavior.
- No implementation code was intentionally modified.

Why it changed:
- Future firmware, slave, MQTT, and Flutter agents need one readable source of truth before coding starts.

Implementation effect:
- Coding agents should use this report as a handoff checklist and should implement only after confirming the relevant specification section.

---

## Master Agent Scope

- [x] Inspect current project docs and repository status.
- [x] Coordinate domain agents through strict documentation scopes.
- [x] Review modified docs for V1/V2 contradictions.
- [x] Create final integration notes.
- [x] Avoid direct source code edits.

Files reviewed:
- `README.md`
- `docs/README.md`
- `docs/FSD_Smart_Building_Master_UPDATED.md`
- `docs/Flutter_App_MQTT_Requirements.md`
- `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md`
- `docs/Smart_Building_RS485_Modbus_Architecture.md`
- `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`
- `docs/UIUX.md`

---

## Documentation Agent

Scope:
- Edit high-level documentation and FSD-related specification text only.
- Do not edit firmware source code.

Completed:
- [x] FSD states Firmware V2 startup flow: `START -> Check Saved Slave -> Try reconnect if saved slave exists -> Else do nothing`.
- [x] FSD documents per-data-type MQTT publish model.
- [x] FSD documents actuator subscribe topics and confirmation-based state republish.
- [x] FSD documents v2.1 Device Profile enforcement, with Lux as an optional assignment where profile policy allows it.
- [x] FSD keeps legacy combined MQTT JSON under Legacy / V1 compatibility.
- [x] FSD examples were adjusted so a slave summary no longer looks like `Temp + CO2 + Presence` as one active role.
- [x] FSD includes RS485 block diagram and register/header reference based on the agreed v2.1 slave contract.

Edited files:
- `README.md`
- `docs/README.md`
- `docs/FSD_Smart_Building_Master_UPDATED.md`

Open items:
- [x] Firmware V2.5 production topic format is finalized as `<class_name>/<data_type>`.
- [ ] Final invalid/stale representation for integer MQTT topics should be confirmed before implementation.

---

## MQTT Specification Agent

Scope:
- Edit only MQTT-related docs/spec files.
- Do not edit firmware implementation code.

Completed:
- [x] `docs/Flutter_App_MQTT_Requirements.md` now describes Firmware V2 as per-topic MQTT.
- [x] Exact runtime publish topics are documented using `<class_name>/<data_type>`.
- [x] General/simple sensor values use integer payloads.
- [x] V2.5 update: temperature publishes integer average Celsius; older JSON-per-position wording is historical.
- [x] V2.5 update: LED publishes integer `1` or `0`; older JSON-per-position wording is historical.
- [x] Subscribe/control examples include LED, AC, and Projector.
- [x] Command flow is documented as: app command -> master -> target slave -> confirmation -> republish state.
- [x] Old single master state JSON is retained only as Legacy / V1 Notes.
- [x] MQTT actuator state and command traffic use the same literal topic with firmware self-echo guards.
- [x] Temperature and LED scalar payload examples no longer include documentation metadata as payload fields.
- [x] Superseded by V2.5/V2.2 draft: AC command payload is now documented as `PPTTFFSS`; Projector command payload is scalar `1` or `0`.
- [x] Class/room naming now drives default topic labels such as `HD01/co2` and `LA2/co2`.
- [x] MQTT QoS/retain policy is documented: simple sensors QoS 0 retain true, temperature average QoS 0 retain true, LED/projector state QoS 1 retain true, actuator commands QoS 1 retain false, master status QoS 1 retain true.

Edited files:
- `docs/Flutter_App_MQTT_Requirements.md`

Open items:
- [ ] MQTT Setup UI still needs implementation to show/apply the documented QoS/retain defaults.

---

## Slave Specification Agent

Scope:
- Edit only slave-related docs/spec files.
- Do not edit MQTT app docs or display/UI files except by reference.

Completed:
- [x] Slave contract remains the agreed source: `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md` v2.1.0.
- [x] Contract documents master-led startup and saved-slave recovery.
- [x] Contract documents master-owned Device Profile enforcement, including `IR_COMBO_NODE` as the production IR combo profile.
- [x] Legacy multi-function device class is clearly marked as legacy.
- [x] Recovery mapping wording was cleaned to `MAC-to-address`.
- [x] Header/config snippets include edit-target/purpose/reason comments.

Edited files:
- `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md`

Open items:
- [x] Lux register representation is defined by v2.1.0: `LUX_SENSOR_ASSIGNMENT 0x0011` and runtime Lux registers `0x0104..0x0107`.
- [x] AC/projector production role is defined by v2.1.0 as `IR_COMBO_NODE`, allowing AC 1, AC 2, and Projector on one IR slave.
- [x] Dedicated one-main-function register is superseded by master-owned Device Profile enforcement.

---

## Flutter/App Requirement Agent

Scope:
- Edit Flutter/app requirement docs and UI requirement docs only.
- Do not edit firmware or slave implementation code.

Completed:
- [x] App model uses Home, Devices, Settings.
- [x] Home shows selected class/room dashboard.
- [x] Devices manages discovered MQTT masters/devices and display settings.
- [x] Settings manages broker/topic configuration.
- [x] App requirements consume per-topic MQTT instead of depending on a single master state JSON.
- [x] LED sync state is defined as confirmed integer `1` or `0` from the LED topic.
- [x] MQTT temperature UI uses the integer average from the temperature topic; fixed positions remain local master UI detail.
- [x] UIUX dashboard input section warns that single master state JSON is Legacy / V1 only.
- [x] Settings/Device Info now includes editable class/room name, firmware version, and `Firmware By Hansel Kay CE LAB`.
- [x] Class/room name changes regenerate default topic labels/templates.

Edited files:
- `docs/Flutter_App_MQTT_Requirements.md`
- `docs/UIUX.md`

Open items:
- [ ] Final Flutter visual design and detailed widget layout are still implementation tasks.
- [ ] Device discovery UX over MQTT may be refined while keeping the finalized
  `<class_name>/<data_type>` topic naming.

---

## Review Agent

Scope:
- Read modified docs.
- Check consistency.
- Do not edit implementation code.

Completed:
- [x] Confirmed Firmware V2/v2.1 concepts appear across docs: saved slave reconnect, per-topic MQTT, actuator confirmation, and master-owned Device Profile policy.
- [x] Confirmed v2.1.0 slave contract is the active slave reference for updated FSD/README work.
- [x] Checked for stale V2.0.0 slave contract references.
- [x] Checked for `PAIRING_HELLO` as an active discovery model.
- [x] Checked that legacy single master JSON remains marked as Legacy / V1.
- [x] Checked that source implementation files under `src/` were not changed by this pass.
- [x] Final master consistency pass updated FSD stale-data behavior to per-topic Firmware V2 behavior.
- [x] Final master consistency pass clarified MQTT state-vs-command direction to prevent command/state self-echo ambiguity.

Review notes:
- `PAIRING_HELLO` appears only as a negative statement in the FSD: discovery SHALL use Modbus address `247`, not unsolicited `PAIRING_HELLO`.
- `smart_building_master_state` appears only in Legacy / V1 compatibility contexts.
- `1 Slave = Multiple Capability` appears only under Legacy / V1 Notes.

Open items:
- [x] Final review rerun after v2.1 defined Lux registers and the AC/projector role as `IR_COMBO_NODE`.

---

## Master Agent V2.1 Contract Alignment Backlog

Scope:
- Align remaining master-side documentation with `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md` v2.1.0.
- Treat v2.1.0 as the new proposed active slave wire contract once approved.
- Do not change firmware source code in this documentation pass unless a later implementation task explicitly requests it.

Contract baseline:
- Active target contract: `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md`
- Version: `v2.1.0`
- Identity registers: `0x0000..0x0004`
- Capability registers: `0x0010..0x0017`
- Config/recovery registers: `0x00F0..0x00F7`
- Runtime sensor/state block: `0x0100..0x010E`
- AC/projector command status registers: `0x0206`, `0x0207`, `0x0212`

Global conflicts to clear:
- [x] Replace references that say `V_1_4_0` is the active/agreed/normative slave contract with v2.1.0 references.
- [x] Replace `NODE_ADDRESS 0x00F0` with `NODE_ADDRESS 0x0000`.
- [x] Remove `SAVE_CONFIG 0x00F1 = 0xA55A` as an active v2 behavior; v2.1.0 removes slave-side save config.
- [x] Replace old config register map `0x00F0..0x00F9` with v2.1.0 map: `CONFIG_VERSION 0x00F0`, `LAST_ERROR 0x00F1`, `UPTIME_LOW 0x00F2`, `UPTIME_HIGH 0x00F3`, recovery `0x00F4..0x00F7`.
- [x] Replace recovery docs that read MAC first at `247` as the normal known-device recovery flow. v2.1.0 recovery writes target MAC/address to `247` and confirms by polling the recovered address.
- [x] Add recovery response-collision handling wherever recovery is documented: master ignores Modbus response collision/error for the recovery write only.
- [x] Split recovery vs discovery everywhere: auto recovery known device is allowed; auto pairing unknown device is forbidden.
- [x] Add `UNPAIRED_DEVICE_DETECTED` state for unknown slaves seen at address `247`.
- [x] Replace capability count register model `0x0011..0x0016` with v2.1.0 assignment/profile model `0x0010..0x0017`.
- [x] Replace one-main-function slave enforcement language with master-owned Device Profile enforcement. Slave is policy-blind.
- [x] Add device profiles where assignment policy is described: `TEMP_NODE`, `PRESENCE_NODE`, `CO2_NODE`, `RELAY_NODE`, `IR_COMBO_NODE`.
- [x] Update Lux docs from "open/no register" to v2.1.0 Lux assignment/runtime registers: `LUX_SENSOR_ASSIGNMENT 0x0011`, `LUX_1_LX..LUX_4_LX 0x0104..0x0107`.
- [x] Update IR docs so AC 1, AC 2, and Projector may coexist under `IR_COMBO_NODE`.
- [x] Update relay register references from old `0x0130..0x0131` to v2.1.0 `0x010D..0x010E`.
- [x] Update CO2 register references from old `0x0110` to v2.1.0 `0x0108`.
- [x] Update presence register references from old `0x0120..0x0121` to v2.1.0 `0x0109..0x010C`.
- [x] Update polling guidance from grouped reads to `Read 0x0100 length 15` plus profile/assignment parsing.
- [x] Update persistent storage wording: master persistent storage may be EEPROM/NVS/Preferences/filesystem abstraction; slave remains RAM-only.
- [x] Split master persistent registry fields from runtime fields: persist MAC, address, profile, device name, room; keep last seen and online status as runtime or low-frequency event persistence.
- [x] Add saved device registry UI actions where missing: rename, delete, recover, re-pair.
- [x] Add AC/projector command feedback docs where actuator flow is described: command status only, not real AC/projector state, because IR is one-way.

File-specific checklist:
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: update normative slave contract reference from `V_1_4_0` to v2.1.0.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: update pairing flow to write capability registers `0x0010..0x0017` and address `0x0000`.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: remove active `SAVE_CONFIG` flow and constants.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: update recovery flow from `0x00F6..0x00F9` to single recovery write `0x00F4..0x00F7`.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: replace capability count section with Device Profile enforcement and policy-blind slave wording.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: update Modbus constants/header block to v2.1.0.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: update runtime register diagrams and examples from old grouped sensor addresses to contiguous `0x0100..0x010E`.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: update Lux text from unresolved/open to v2.1.0-assigned registers.
- [x] `docs/FSD_Smart_Building_Master_UPDATED.md`: update LED/relay compatibility note from `0x0130..0x0131` to `0x010D..0x010E`.
- [x] `docs/UIUX.md`: replace one-main-sensor unavailable UI rule with Device Profile selection/enforcement UI.
- [x] `docs/UIUX.md`: update SAVE action from count registers `0x0011..0x0016` and `SAVE_CONFIG` to v2.1.0 `0x0010..0x0017` plus no slave save signal.
- [x] `docs/UIUX.md`: add unknown device state `UNPAIRED_DEVICE_DETECTED` and user-initiated pair flow.
- [x] `docs/UIUX.md`: add saved registry actions: rename, delete, recover, re-pair.
- [x] `docs/Smart_Building_RS485_Modbus_Architecture.md`: update active wire contract path/version from `V_1_4_0` to v2.1.0.
- [x] `docs/Smart_Building_RS485_Modbus_Architecture.md`: update address assignment, config/recovery register map, and polling pattern to v2.1.0.
- [x] `docs/Smart_Building_RS485_Modbus_Architecture.md`: replace count-register capability philosophy with v2.1.0 assignment registers and master-owned profile policy.
- [x] `docs/Smart_Building_RS485_Modbus_Architecture.md`: update old sensor register sections: CO2, presence, relay, Lux, and one-shot sensor block.
- [x] `docs/Smart_Building_RS485_Modbus_Architecture.md`: remove "Lux remains open" and "AC/projector split needs confirmation" because v2.1.0 defines Lux and `IR_COMBO_NODE`.
- [x] `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`: update contract reference and remove `V_1_4_0` Lux-open language.
- [x] `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`: replace one-main-type UI rules with Device Profile model and `IR_COMBO_NODE` exception.
- [x] `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`: update capability write docs from `0x0011..0x0016` and `SAVE_CONFIG` to `0x0010..0x0017`.
- [x] `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`: update address assignment from `0x00F0` to `0x0000`.
- [x] `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`: update runtime mapping examples to contiguous v2.1.0 sensor/state block.
- [x] `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`: add saved registry fields and runtime last-seen/status distinction if not already explicit.
- [x] `docs/Flutter_App_MQTT_Requirements.md`: review whether Device Profile names or saved-device fields need to be exposed to the app/device list docs.
- [x] `docs/README.md`: update any slave-contract pointer from `V_1_4_0` to v2.1.0.
- [x] `README.md`: update any high-level slave-contract pointer from `V_1_4_0` to v2.1.0.

Verification checklist:
- [x] `rg "V_1_4_0|V1_4|0x00F8|0x00F9|SAVE_CONFIG|0xA55A|TEMP_SENSOR_COUNT|LCD_CTRL_COUNT|0x0130|0x0110|0x0120|Lux remains|Lux register representation is not defined|one main sensor type plus optional Lux" docs` returns only legacy/changelog/backlog entries or no active contradictions.
- [x] `rg "RS485_Modbus_Slave_Firmware_Contract v2.md|v2.1.0|IR_COMBO_NODE|UNPAIRED_DEVICE_DETECTED|0x00F4..0x00F7|0x0010..0x0017|0x0100..0x010E" docs` confirms the new model is documented in all affected specs.
- [x] Final human review confirms recovery is automatic and discovery/pairing is user initiated.
- [x] Final human review confirms slave remains RAM-only and policy-blind.

---

## Master Agent Orchestration Plan - V2.1 Documentation Alignment

Master Agent role:
- Read the current FSD and slave contract v2.1 before assigning work.
- Keep each agent inside its documented scope.
- Prevent agents from editing the same file at the same time.
- Verify each agent's output against the v2.1 slave contract before marking it done.
- Update this checklist as agents complete their work.

Baseline read:
- [x] Master Agent read `docs/FSD_Smart_Building_Master_UPDATED.md`.
- [x] Master Agent read `docs/From_SLave/RS485_Modbus_Slave_Firmware_Contract v2.md`.
- [x] Master Agent confirmed FSD/README/doc index are already aligned to v2.1 for the updated scope.

Shared contract rules all agents must follow:
- [x] Use slave contract v2.1.0 as active wire contract.
- [x] Use `NODE_ADDRESS 0x0000`.
- [x] Use capability/profile registers `0x0010..0x0017`.
- [x] Use config/recovery registers `0x00F0..0x00F7`.
- [x] Use recovery write `247:0x00F4 length 4`, then confirm by polling assigned address.
- [x] Treat recovery as automatic for known devices.
- [x] Treat unknown devices at address `247` as `UNPAIRED_DEVICE_DETECTED`.
- [x] Keep pairing/discovery for unknown devices user initiated.
- [x] Remove active slave-side `SAVE_CONFIG` behavior.
- [x] Use runtime sensor/state block `0x0100..0x010E`.
- [x] Use Device Profile enforcement in master, not slave.
- [x] Keep slave RAM-only and policy-blind.
- [x] Use `IR_COMBO_NODE` for AC 1, AC 2, and Projector on one IR slave.
- [x] Keep current AC panel behavior: one AC control mirrors power/temp/mode to AC 1 and AC 2 when both are exposed.

Agent execution order:

1. Architecture Agent.
2. Connectivity Mapping Agent.
3. UIUX Agent.
4. MQTT/App Agent.
5. Master Review Agent.

Agent ownership rules:
- Only one agent edits a document at a time.
- Architecture Agent owns `docs/Smart_Building_RS485_Modbus_Architecture.md`.
- Connectivity Mapping Agent owns `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md`.
- UIUX Agent owns `docs/UIUX.md`.
- MQTT/App Agent owns `docs/Flutter_App_MQTT_Requirements.md`.
- Master Review Agent may read all docs, but should not rewrite broad sections unless it is fixing contradictions found during final verification.
- Agents SHALL NOT edit firmware source files during this documentation-alignment pass.
- Agents SHALL NOT reintroduce `V_1_4_0` as active contract.

### Agent Checklist

FSD / README Alignment Agent:
- [x] Updated FSD normative slave contract reference to v2.1.0.
- [x] Updated FSD pairing, recovery, register map, Device Profile, Lux, relay, and AC mirror notes.
- [x] Updated `README.md` slave contract pointer and register summary to v2.1.
- [x] Updated `docs/README.md` slave contract pointer and v2.1 summary.
- [x] Ran targeted scan for old active conflicts in FSD/README/doc index.

Architecture Agent:
- [x] Update active wire contract path/version from `V_1_4_0` to v2.1.0.
- [x] Update pairing/address assignment from `0x00F0` to `0x0000`.
- [x] Update recovery from `0x00F6..0x00F9` to `0x00F4..0x00F7`.
- [x] Replace count-register capability philosophy with v2.1 assignment registers and master-owned Device Profile policy.
- [x] Update runtime register descriptions to contiguous `0x0100..0x010E`.
- [x] Remove "Lux remains open" and replace with v2.1 Lux registers.
- [x] Replace AC/projector open question with `IR_COMBO_NODE`.
- [x] Add response-collision note for recovery write at address `247`.
- [x] Mark Architecture Agent done only after targeted `rg` scan passes for this file.

Connectivity Mapping Agent:
- [x] Update active contract reference to v2.1.0.
- [x] Replace one-main-type UI rules with Device Profile model and `IR_COMBO_NODE` exception.
- [x] Update capability writes from `0x0011..0x0016` to `0x0010..0x0017`.
- [x] Remove active `SAVE_CONFIG` behavior.
- [x] Update address assignment from `0x00F0` to `0x0000`.
- [x] Update runtime mapping examples to v2.1 sensor/state block `0x0100..0x010E`.
- [x] Update Lux mapping from future/open to v2.1 Lux registers.
- [x] Add `UNPAIRED_DEVICE_DETECTED` for unknown device flow.
- [x] Add saved registry fields: MAC, address, Device Profile, device name, room; keep last seen/status as runtime or low-frequency persistence.
- [x] Mark Connectivity Mapping Agent done only after targeted `rg` scan passes for this file.

UIUX Agent:
- [x] Replace one-main-sensor unavailable UI rule with Device Profile selection/enforcement UI.
- [x] Show Device Profile choices: `TEMP_NODE`, `PRESENCE_NODE`, `CO2_NODE`, `RELAY_NODE`, `IR_COMBO_NODE`.
- [x] Represent `IR_COMBO_NODE` as AC 1, AC 2, and Projector-capable.
- [x] Keep current dashboard AC control as one panel, with AC 1+2 mirrored behavior documented as temporary.
- [x] Update SAVE behavior to write v2.1 registers `0x0010..0x0017`.
- [x] Remove active slave-side `SAVE_CONFIG` UI/action wording.
- [x] Add unknown device state `UNPAIRED_DEVICE_DETECTED` and user-initiated Pair Device flow.
- [x] Add saved registry actions: Rename, Delete, Recover, Re-Pair.
- [x] Mark UIUX Agent done only after targeted `rg` scan passes for this file.

MQTT/App Agent:
- [x] Review whether Device Profile should appear in Flutter/device list docs.
- [x] Update app-facing device metadata if needed: Device Profile, room, name, status, last seen.
- [x] Confirm AC command docs reflect one AC panel for now, mirrored to AC 1 and AC 2 by master when both are exposed.
- [x] Confirm IR command status is command-result only, not actual AC/projector state.
- [x] Ensure no MQTT docs depend on old slave register map.
- [x] Mark MQTT/App Agent done only after targeted `rg` scan passes for this file.

Master Review Agent:
- [x] Run cross-doc scan for old active contract references after all agents finish.
- [x] Confirm `V_1_4_0` appears only in legacy/changelog/backlog context, not active requirements.
- [x] Confirm no active docs require `SAVE_CONFIG`.
- [x] Confirm no active docs use old recovery `0x00F6..0x00F9`.
- [x] Confirm no active docs use old count-register capability model as v2.1 primary behavior.
- [x] Confirm no active docs say Lux is undefined/open.
- [x] Confirm no active docs treat AC/projector split as unresolved.
- [x] Confirm all affected docs mention Device Profile or point to a doc that does.
- [x] Mark final review complete only after all blocking contradictions are cleared.

Master Agent status board:

| Agent | Status | Files |
| --- | --- | --- |
| FSD / README Alignment Agent | Done | `docs/FSD_Smart_Building_Master_UPDATED.md`, `README.md`, `docs/README.md` |
| Architecture Agent | Done | `docs/Smart_Building_RS485_Modbus_Architecture.md` |
| Connectivity Mapping Agent | Done | `docs/Smart_Building_Connectivity_Dashboard_Mapping_Design_UPDATED.md` |
| UIUX Agent | Done | `docs/UIUX.md` |
| MQTT/App Agent | Done | `docs/Flutter_App_MQTT_Requirements.md` |
| Master Review Agent | Done | all docs |

Master verification commands:

```powershell
rg -n "V_1_4_0|V1_4|0x00F8|0x00F9|SAVE_CONFIG|0xA55A|TEMP_SENSOR_COUNT|LCD_CTRL_COUNT|0x0130|0x0110|0x0120|Lux remains|Lux register representation is not defined|one main sensor type plus optional Lux" docs README.md
rg -n "RS485_Modbus_Slave_Firmware_Contract v2.md|v2.1.0|IR_COMBO_NODE|UNPAIRED_DEVICE_DETECTED|0x00F4..0x00F7|0x0010..0x0017|0x0100..0x010E|Device Profile" docs README.md
```

Master note:
- Agents are marked done after scoped edits and targeted scans confirmed no active v2.1 contradictions in the owned docs.
- Remaining matches for old terms are historical, changelog, backlog, or explicit removed-behavior notes.

---

## Deprecated V1 Assumptions

- [x] One combined MQTT state JSON as the primary app contract.
- [x] One combined MQTT command JSON as the primary command contract.
- [x] Fresh slave assignment on every boot.
- [x] One slave actively assigned to multiple unrelated main sensor/control groups.
- [x] Unsolicited binary `PAIRING_HELLO` discovery.
- [x] `0x0010` capability mask as the active slave capability contract.

---

## Implementation Impact Notes

Firmware:
- Startup must check saved slave registry before discovery.
- MQTT manager must publish per-topic payloads and subscribe to actuator command topics.
- Actuator commands must be confirmed by the target slave before state republish.

Slave:
- Slave boots at address `247` because config is RAM-only.
- Master recovery uses one write to `0x00F4..0x00F7` at address `247`, then confirms by polling the recovered assigned address.
- Capability/profile assignment uses v2.1 registers `0x0010..0x0017`.
- Master owns Device Profile enforcement; slave remains policy-blind.

MQTT:
- Simple sensor payloads are integers.
- Temperature payload is integer average Celsius. `-1` means no valid temperature.
- LED payload is integer `1` or `0`.
- Runtime topic names like `HD01/suhu` are generated from the editable
  class/room name using `<class_name>/<data_type>`.

Flutter app:
- App should render Home, Devices, and Settings.
- Home should build a dashboard from per-topic updates.
- LED and actuator controls should show pending state until confirmed state is republished.

---

## Firmware V2.1 Implementation Agent Pass

Scope:
- Apply the approved v2.1 slave contract and master FSD behavior into firmware code.
- Keep documentation edits separate from firmware source edits.
- Build and flash after integration.

Agent checklist:
- [x] RS485 Worker updated `src/rs485_manager.h` and `src/rs485_manager.cpp`.
- [x] Master Agent reviewed and integrated the RS485 Worker output.
- [x] UI/MQTT Worker started the data/UI/MQTT surface update; Master Agent completed integration after the worker was stopped.
- [x] Master Agent updated `src/data.h` and `src/data.cpp` for Device Profile, registry status, room/name metadata, and command queues.
- [x] Master Agent updated `src/mqtt_manager.cpp` for per-topic v2 state, registry metadata, `UNPAIRED_DEVICE_DETECTED`, and RS485 command enqueueing.
- [x] Master Agent updated `src/ui_screens.h` and `src/ui_screens.cpp` for Device Info, Device Profile selection, unknown-device state, and AC1+AC2 mirror semantics.

Implemented:
- [x] v2.1 Modbus register map: identity `0x0000..0x0004`, capability `0x0010..0x0017`, recovery `0x00F4..0x00F7`, runtime block `0x0100..0x010E`.
- [x] Removed active slave-side `SAVE_CONFIG` write path from firmware.
- [x] Known-device recovery writes MAC/address to `247:0x00F4` length `4`, ignores the recovery-write response, then confirms by polling the assigned address.
- [x] Unknown devices at address `247` surface as `UNPAIRED_DEVICE_DETECTED` instead of automatic pairing.
- [x] Device Profile model is master-owned: `TEMP_NODE`, `PRESENCE_NODE`, `CO2_NODE`, `RELAY_NODE`, `IR_COMBO_NODE`.
- [x] `IR_COMBO_NODE` enables AC 1, AC 2 mirror behavior and Projector IR on one slave.
- [x] MQTT master status publishes registry metadata: device name, room, MAC, address, profile, status, and last seen.
- [x] UI and MQTT actuator commands enqueue RS485 writes for LED, AC, and Projector.

Verification:
- [x] `platformio run` succeeded.
- [x] Active source scan for stale v1.4 register/save-config patterns returned no matches.
- [x] `platformio run -t upload --upload-port COM4` succeeded.

Flash result:
- Port: `COM4`
- Chip: ESP32-S3
- MAC: `e0:72:a1:f2:cf:cc`
- Result: success, hard reset via RTS.

---

## Final Confirmation

- [x] Documentation pass stayed scoped to docs/specification files.
- [x] Firmware implementation pass intentionally modified source files after user approval.
- [x] Agentic code work was reviewed by Master Agent before build and flash.
- [x] Remaining product-level open items are documented as open items instead of guessed architecture.
