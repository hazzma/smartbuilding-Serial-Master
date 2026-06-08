# Automation & Security Update Specification

This document details the system design, logical changes, and documentation impact for the new automation, feedback-loop verification, and occupancy-aware safety override features.

---

## 1. Projector State Verification using Lux Sensor (BH1750)

### Background
Currently, the Projector IR control operates as a one-way (simplex) transmission. The Master cannot verify if the projector actually powered ON or if it failed (e.g., due to bulb burnout or blocked IR transceiver). By utilizing the ambient Lux sensor (BH1750) on the slave device, the Master can verify the projector's power state through a light-level feedback loop.

### Logic & State Machine
1. **State Addition**: A new state `PROJ_STATE_POWERING_ON` is introduced alongside `PROJ_STATE_OFF` and `PROJ_STATE_ON`.
2. **Initial Trigger**:
   - When the user (via local HMI or MQTT control) triggers Projector ON:
     1. Record the current ambient light level as $L_{initial}$ (in Lux).
     2. Transition the internal state to `PROJ_STATE_POWERING_ON`.
     3. Send the IR ON Modbus command to the slave.
     4. Immediately publish `1` (ON) to MQTT topic `HD01/data/projector` to maintain responsive UI status.
     5. Start an 8-second `warmup_timer`.
3. **Verification**:
   - After the 8-second `warmup_timer` expires:
     1. Read the latest Lux value as $L_{current}$.
     2. Calculate the difference: $\Delta L = L_{current} - L_{initial}$.
     3. If $\Delta L \ge L_{threshold}$ (default: $50\text{ lx}$):
        - Transition to `PROJ_STATE_ON`.
     4. If $\Delta L < L_{threshold}$:
        - If `retry_count == 0`:
          - Increment `retry_count` to `1`.
          - Re-send the IR ON command to the slave.
          - Restart the 8-second timer.
        - If `retry_count == 1`:
          - Transition to `PROJ_STATE_OFF`.
          - Set the **Projector Alert Flag** (Bit 5 in alert bitmask) to report a hardware failure.
          - Publish `0` (OFF) to MQTT topic `HD01/data/projector` (reverting the initial optimistic state).
          - Reset `retry_count` to `0`.

---

## 2. Scheduler & Occupancy-Based Shutdown (Smart Shutdown)

### Logic & Event Flow
1. **Pre-Class Event (Server to Master)**:
   - Topic: `HD01/control/schedule`
   - Payload: `"PRE_CLASS_ON"` (sent 20 minutes before a scheduled class).
   - Action: Master immediately sends command to turn ON the AC and Lights.
2. **Class Ended Event (Server to Master)**:
   - Topic: `HD01/control/schedule`
   - Payload: `"CLASS_ENDED"` (sent at the exact end of class).
   - Action: Master starts a local 20-minute countdown timer (`shutdown_timer`).
3. **Shutdown Verification**:
   - When the 20-minute `shutdown_timer` expires, the Master checks the human presence state:
     - **If `human_presence == false`** (Class is empty):
       - Master sends commands to turn OFF the AC and Lights.
     - **If `human_presence == true`** (Students or teacher still present):
       - Master delays shutdown and schedules a recheck every 5 minutes.
       - Once human presence becomes `false` during a recheck, the Master turns OFF the AC and Lights.

---

## 3. Acceptation Level (Local Occupancy Override)

### Background
To prevent remote scheduling scripts or server-side operators from disrupting active classes (e.g., turning off lights or AC while students are studying or taking exams), the Master enforces a local occupancy safety check.

### Override Rules
When `g_state.sensor.human_presence == true` (Classroom is occupied):
1. **MQTT LED Commands Restricted**:
   - Ignore any incoming commands on `HD01/control/led` that attempt to toggle LED power (both ON and OFF commands from the server are discarded).
2. **MQTT AC Commands Restricted**:
   - Ignore any command on `HD01/control/ac` that attempts to turn the AC OFF.
   - Accept commands that adjust target temperature, modes, fan speed, or swing.
3. **Local HMI Control Priority**:
   - Local touchscreen controls on the Master HMI remain fully enabled. Physical users inside the classroom can override the safety block at any time.

When `g_state.sensor.human_presence == false` (Classroom is empty):
- All server-side MQTT control commands (ON, OFF, parameter changes) are accepted and executed.

---

## 4. 7-Day Rolling Average Lamp Anomaly Alerting

### Background
Detects electrical anomalies (e.g., lights left ON overnight or left ON in an empty room) by monitoring daily active durations, without exhausting ESP32 flash memory or requiring cloud calculations.

### Technical Implementation
1. **Daily Tracking**:
   - The Master tracks the cumulative "ON duration" (in minutes) of the light relays for the current day in RAM (`current_day_duration`).
2. **Midnight Rolling Log**:
   - The Master synchronizes its clock via NTP.
   - At exactly 00:00 (midnight) or on boot if a date boundary is crossed:
     1. Write `current_day_duration` to the circular history buffer:
        `light_history[day_count % 7] = current_day_duration;`
     2. Increment `day_count`.
     3. Reset `current_day_duration` to `0`.
     4. Persist `light_history` (7 elements of `uint16_t`) and `day_count` (uint32_t) to ESP32 Preferences under the namespace `"light_anom"`. This ensures day 1 is replaced by day 8, maintaining a sliding 7-day window.
3. **Anomaly Logic**:
   - Preequisite: `day_count >= 7` (requires at least 1 week of historical baseline).
   - If `current_day_duration > (average(light_history) * 1.5)` (exceeds 1.5x of the weekly average) AND the current time is after hours (e.g., 22:00 to 06:00) AND `human_presence == false`:
     - Raise **Light Anomaly Alert** by setting **Bit 7** (value `128`) on the `HD01/data/alert` bitmask.

---

## 5. Documentation Impact Analysis

The following files require updates to integrate these new features:

### 5.1 [FSD_Smart_Building_Master_UPDATED.md](file:///c:/Users/hanse/Documents/PlatformIO/Projects/S3%20Master%20Serial/docs/FSD_Smart_Building_Master_UPDATED.md)
* **Section 1.1 (Firmware V2 Change Summary)**:
  - Add details about the Projector verification loop, Acceptation Level rules, schedule processing, and rolling anomaly checks.
* **Section 5.3 (Screen State Machine)**:
  - Update the dashboard state transitions to include the `Powering On` state for the projector.
* **Section 6 (Shared State Requirements)**:
  - Add variables for projector verification states (`warmup_timer`, `retry_count`, `lux_initial`), scheduler countdowns, and rolling lamp history arrays.
* **Section 7 (Functional Requirements)**:
  - Add new requirements:
    - `CTRL-010` (Projector verification algorithm).
    - `CTRL-011` (Acceptation Level override logic).
    - `NET-011` (Schedule packet processor).
    - `ALRT-002` (7-Day Lamp anomaly alerting logic).

### 5.2 [UIUX.md](file:///c:/Users/hanse/Documents/PlatformIO/Projects/S3%20Master%20Serial/docs/UIUX.md)
* **Section 11 (Projector Widget Behavior)**:
  - Document the `"Powering On"` visual state: the widget should display a pulsing/progress indicator or auxiliary badge color when `PROJ_STATE_POWERING_ON` is active.
  - Document fallback behavior: if the verification fails, the widget returns to the `"OFF"` visual state with an alert icon.

### 5.3 [MQTT_V2.5_Changelog.md](file:///c:/Users/hanse/Documents/PlatformIO/Projects/S3%20Master%20Serial/docs/MQTT_V2.5_Changelog.md)
* **Section Runtime Topic Format**:
  - Register `HD01/control/schedule` commands: `"PRE_CLASS_ON"`, `"CLASS_ENDED"`.
* **Section Payload Format (Alert Decimal Bitmask Table)**:
  - Add Bit 7 (Value `128`) for "Light Anomaly Alert".
