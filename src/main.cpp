#include <Arduino.h>
#include "display.h"
#include "data.h"
#include "ui_screens.h"
#include "ui_widgets.h"
#include "touch.h"
#include <WiFi.h>
#include "mqtt_manager.h"
#include "lan_manager.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "rs485_manager.h"
#include "mapping_manager.h"

// Set to 1 to inject a local dummy RS485 slave for UI/home-screen testing.
// Set to 0 to remove the dummy slave and use only real discovered devices.
#define ENABLE_DUMMY_RS485_SLAVE 0

static const uint64_t DUMMY_RS485_MAC = 0xD00D00000123ULL;

static void apply_dummy_rs485_slave(bool enabled) {
    data_lock(g_state);

    bool changed = false;
    uint8_t count = g_state.rs485.slave_count;
    if (count > RS485_MAX_SLAVES) count = RS485_MAX_SLAVES;

    if (!enabled) {
        for (uint8_t i = 0; i < count; i++) {
            if (g_state.rs485.slaves[i].uid != RS485_DUMMY_UI_UID) continue;

            for (uint8_t j = i; j + 1 < count; j++) {
                g_state.rs485.slaves[j] = g_state.rs485.slaves[j + 1];
            }
            memset(&g_state.rs485.slaves[count - 1], 0, sizeof(g_state.rs485.slaves[count - 1]));
            g_state.rs485.slave_count = count > 1 ? count - 1 : 1;
            changed = true;
            break;
        }

        if (changed || g_state.rs485.slave_count == 0) {
            if (g_state.rs485.slave_count == 0) g_state.rs485.slave_count = 1;
            if (g_state.rs485.slaves[0].address == 0 && g_state.rs485.slaves[0].uid == 0) {
                strcpy(g_state.rs485.slaves[0].name, "Device 0");
            }
        }
    } else {
        uint8_t index = RS485_MAX_SLAVES;
        for (uint8_t i = 0; i < count; i++) {
            if (g_state.rs485.slaves[i].uid == RS485_DUMMY_UI_UID) {
                index = i;
                break;
            }
        }

        if (index >= RS485_MAX_SLAVES) {
            if (count == 1 && g_state.rs485.slaves[0].address == 0 && g_state.rs485.slaves[0].uid == 0) {
                index = 0;
            } else if (count < RS485_MAX_SLAVES) {
                index = count;
                g_state.rs485.slave_count = count + 1;
            }
        }

        if (index < RS485_MAX_SLAVES) {
            RS485SlaveState& slave = g_state.rs485.slaves[index];
            bool existing_dummy = slave.uid == RS485_DUMMY_UI_UID;
            uint16_t previous_enabled_mask = existing_dummy ? slave.enabled_mask : 0;
            uint8_t previous_temp_enabled_mask = existing_dummy ? slave.temp_enabled_mask : 0;
            memset(&slave, 0, sizeof(slave));
            slave.address = 0x10;
            slave.uid = RS485_DUMMY_UI_UID;
            slave.mac = DUMMY_RS485_MAC;
            strcpy(slave.name, "Dummy UI Node");
            slave.capability = CAP_TEMP | CAP_CO2 | CAP_HUMAN_PRESENCE |
                               CAP_AC_IR | CAP_PROJECTOR_IR | CAP_LIGHT_RELAY |
                               CAP_LUX | CAP_LCD_CTRL;
            slave.enabled_mask = existing_dummy ?
                                 (previous_enabled_mask & slave.capability) :
                                 slave.capability;
            slave.protocol_version = 1;
            slave.device_class = 1;
            slave.fw_version = 100;
            slave.temp_count = 4;
            slave.temp_enabled_mask = existing_dummy ? (previous_temp_enabled_mask & 0x0F) : 0x0F;
            slave.co2_count = 1;
            slave.presence_count = 1;
            slave.relay_count = 1;
            slave.ir_count = 2;
            slave.lux_count = 1;
            slave.lcd_count = 1;
            slave.temp[0] = 24.8f;
            slave.temp[1] = 25.1f;
            slave.temp[2] = 24.6f;
            slave.temp[3] = 25.4f;
            for (uint8_t i = 0; i < DASHBOARD_TEMP_SLOTS; i++) slave.temp_valid[i] = true;
            slave.co2 = 720;
            slave.co2_valid = true;
            slave.lux = 420.0f;
            slave.lux_valid = true;
            slave.human_presence = true;
            slave.human_presence_valid = true;
            slave.identity_synced = true;
            slave.capability_synced = true;
            slave.last_identity_ms = millis();
            slave.last_capability_ms = millis();
            slave.last_seen = millis();
            slave.online = true;
            slave.degraded = false;
            changed = true;
        }
    }

    if (changed) {
        mapping_manager_update_locked(g_state);
        g_state.ui_needs_update = true;
    }

    data_unlock(g_state);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task_Net: Core 0 — Data & Logic Engine (Non-Visual)
// ─────────────────────────────────────────────────────────────────────────────
void Task_Net(void* pvParameters) {
    uint32_t last_sim_update = 0;
    uint32_t last_dummy_refresh = 0;

    wifi_manager_init();
    wifi_manager_load_and_connect();
    lan_manager_load_config();
    time_manager_init();
    mqtt_init();

    for (;;) {
        uint32_t now = millis();
        static int last_prio = -1;

        if (ENABLE_DUMMY_RS485_SLAVE == 1 && now - last_dummy_refresh > 1000) {
            last_dummy_refresh = now;
            apply_dummy_rs485_slave(true);
        }

        // Unified state update
        data_lock(g_state);
        int current_prio = g_state.net.net_priority;
        bool wifi_now_connected = (WiFi.status() == WL_CONNECTED);
        if (g_state.net.wifi_connected != wifi_now_connected) {
            g_state.net.wifi_connected = wifi_now_connected;
            g_state.ui_needs_update = true;
        }
        bool lan_initialized = g_state.net.lan_initialized;

        // Data Timeout Check (10s) — FSD NET-003
        if (now - g_state.last_data_ts > 10000 && !g_state.use_dummy) {
            bool changed = (g_state.sensor.temp[0] != -100.0f);
            for (int i = 0; i < 4; i++) g_state.sensor.temp[i] = -100.0f;
            g_state.sensor.lux = -1.0f;
            g_state.sensor.co2 = -1;
            if (changed) g_state.ui_needs_update = true;
            g_state.last_data_ts = now;
        }

        // Connection status string update
        if (g_state.net.mqtt_ok) {
            strncpy(g_state.net.conn_status, "CONNECTED", 31);
        } else if (g_state.net.wifi_connected || g_state.net.lan_connected) {
            strncpy(g_state.net.conn_status, "Connecting MQTT", 31);
        } else {
            strncpy(g_state.net.conn_status, "DISCONNECTED", 31);
        }

        // Simulation (if use_dummy)
        if (g_state.use_dummy && (now - last_sim_update > 500)) {
            last_sim_update = now;
            g_state.sensor.temp[0] += 0.01f;
            if (g_state.sensor.temp[0] > 35.0f) g_state.sensor.temp[0] = 20.0f;
            g_state.ui_needs_update = true;
        }
        data_unlock(g_state);

        if (current_prio != last_prio) {
            last_prio = current_prio;
            wifi_manager_set_power(current_prio == 0);
        }

        if (current_prio == 1 && !lan_initialized) {
            lan_init();
        }

        wifi_manager_loop();
        if (current_prio == 1) {
            lan_loop();
        }
        mqtt_loop();
        time_manager_update();

        // 1-second logic check for active-load accumulation, midnight rollover, and scheduler countdowns
        static uint32_t last_sec_check = 0;
        if (now - last_sec_check >= 1000) {
            last_sec_check = now;

            bool save_needed = false;
            bool trigger_rs485_ac_off = false;
            bool trigger_rs485_light_off = false;
            float target_temp = 23.0f;
            uint8_t fan_speed = 0;
            uint8_t swing_mode = 0;

            data_lock(g_state);

            // Accumulate daily active durations.
            if (g_state.sensor.light_on) {
                g_state.sensor.light_accum_sec_today++;
            }
            if (g_state.sensor.light_on || g_state.sensor.ac_on) {
                g_state.sensor.active_load_accum_sec_today++;
            }

            // Scheduler Shutdown Countdown
            if (g_state.sensor.sched_shutdown_active && millis() >= g_state.sensor.sched_shutdown_timer_ms) {
                bool presence_valid = g_state.rs485.dashboard.human_presence_valid;
                bool occupied = presence_valid && g_state.sensor.human_presence;

                if (presence_valid && !occupied) {
                    g_state.sensor.ac_on = false;
                    g_state.sensor.light_on = false;
                    g_state.sensor.sched_shutdown_active = false;
                    g_state.sensor.sched_shutdown_timer_ms = 0;
                    g_state.ui_needs_update = true;

                    trigger_rs485_ac_off = true;
                    trigger_rs485_light_off = true;
                    target_temp = g_state.sensor.temp_target;
                    fan_speed = g_state.sensor.ac_fan_speed;
                    swing_mode = g_state.sensor.ac_swing_mode;
                } else {
                    g_state.sensor.sched_shutdown_timer_ms = millis() + (5 * 60 * 1000);
                    g_state.ui_needs_update = true;
                }
            }

            // NTP time checks & Midnight Rollover
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 5)) {
                // Dynamic active-load anomaly detection (needs enough history and reliable occupancy)
                bool presence_valid = g_state.rs485.dashboard.human_presence_valid;
                bool confirmed_empty = presence_valid && !g_state.sensor.human_presence;
                if (g_state.sensor.light_day_count >= 5 && presence_valid) {
                    uint32_t sum_min = 0;
                    uint8_t valid_days = (g_state.sensor.light_day_count >= 7) ? 7 : g_state.sensor.light_day_count;
                    for (uint8_t i = 0; i < valid_days; i++) {
                        sum_min += g_state.sensor.light_history_min[i];
                    }
                    float avg_min = (valid_days > 0) ? ((float)sum_min / (float)valid_days) : 0.0f;
                    float today_min = g_state.sensor.active_load_accum_sec_today / 60.0f;
                    bool after_hours = (timeinfo.tm_hour >= 22 || timeinfo.tm_hour < 6);
                    float threshold_min = avg_min * 1.5f;
                    float plus_60_min = avg_min + 60.0f;
                    if (threshold_min < plus_60_min) threshold_min = plus_60_min;

                    bool old_alert = g_state.sensor.light_anomaly_alert;
                    g_state.sensor.light_anomaly_alert =
                        (avg_min >= 30.0f) &&
                        (today_min > threshold_min) &&
                        after_hours &&
                        confirmed_empty;
                    if (old_alert != g_state.sensor.light_anomaly_alert) {
                        g_state.ui_needs_update = true;
                        save_needed = true;
                    }
                } else {
                    g_state.sensor.light_anomaly_alert = false;
                }

                // Midnight Rollover checking
                static int last_day = -1;
                if (last_day == -1) {
                    last_day = timeinfo.tm_mday;
                } else if (timeinfo.tm_mday != last_day) {
                    last_day = timeinfo.tm_mday;

                    uint16_t light_min = g_state.sensor.active_load_accum_sec_today / 60;
                    uint32_t current_day = g_state.sensor.light_day_count;
                    g_state.sensor.light_history_min[current_day % 7] = light_min;
                    g_state.sensor.light_day_count++;
                    g_state.sensor.light_accum_sec_today = 0;
                    g_state.sensor.active_load_accum_sec_today = 0;
                    g_state.ui_needs_update = true;
                    save_needed = true;

                    Serial.printf("[Rollover] Midnight transition. Saved active load: %u min. Total days: %u\n",
                                  light_min, (unsigned)g_state.sensor.light_day_count);
                }
            }

            data_unlock(g_state);

            // Trigger commands outside lock
            if (trigger_rs485_ac_off) {
                rs485_request_ac_command(false, target_temp, 0, fan_speed, swing_mode);
            }
            if (trigger_rs485_light_off) {
                rs485_request_light_command(false);
            }
            if (trigger_rs485_ac_off || trigger_rs485_light_off || save_needed) {
                if (save_needed) {
                    data_save_device_config(g_state);
                }
                mqtt_publish_state();
            }
        }

        static uint32_t last_hb = 0;
        if (now - last_hb > 5000) {
            last_hb = now;
            Serial.printf("[NET] Alive | Prio:%d | MQTT:%s | Heap:%u | Stack:%u\n",
                          current_prio,
                          is_mqtt_connected() ? "OK" : "FAIL",
                          (unsigned)ESP.getFreeHeap(),
                          (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task_Touch: Core 1 — FT6236U I2C Capacitive Touch Polling (20ms / 50Hz)
// ─────────────────────────────────────────────────────────────────────────────
void Task_Touch(void* pvParameters) {
    int tx, ty;
    TouchEventType event;
    for (;;) {
        if (touch_get_event(tx, ty, event)) {
            screens_handle_touch_event(g_state, tx, ty, event);
            data_lock(g_state);
            g_state.ui_needs_update = true;
            data_unlock(g_state);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task_UI: Core 1 — LovyanGFX Sprite Render Engine (High Priority)
// ─────────────────────────────────────────────────────────────────────────────
void Task_UI(void* pvParameters) {
    int      frames        = 0;
    int      current_fps   = 0;
    uint32_t last_time     = millis();
    uint32_t last_frame_ts = 0;
    uint32_t total_render  = 0;
    uint32_t total_push    = 0;

    const int target_frame_ms = 25; // ~40 FPS

    for (;;) {
        uint32_t now = millis();

        bool needs_update = false;
        data_lock(g_state);
        needs_update = g_state.ui_needs_update;
        static uint32_t last_force = 0;
        if (now - last_force > 2000) { needs_update = true; last_force = now; }
        data_unlock(g_state);
        if (screens_has_animation()) needs_update = true;

        if (needs_update && (now - last_frame_ts >= (uint32_t)target_frame_ms)) {
            last_frame_ts = now;

            if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(target_frame_ms)) == pdTRUE) {
                uint32_t t0 = micros();
                screens_render(g_state, current_fps);
                total_render += (micros() - t0);

                t0 = micros();
                p_engine->pushToDisplay();
                total_push += (micros() - t0);

                widgets_swap();
                xSemaphoreGive(bus_mutex);
                frames++;

                data_lock(g_state);
                g_state.ui_needs_update = false;
                data_unlock(g_state);
            }
        }

        if (millis() - last_time >= 1000) {
            current_fps = frames;
            uint32_t avg_r = (frames > 0) ? (total_render / frames / 1000) : 0;
            uint32_t avg_p = (frames > 0) ? (total_push   / frames / 1000) : 0;
            Serial.printf("[UI] FPS: %d | Render: %dms | Push: %dms\n",
                          current_fps, avg_r, avg_p);
            frames = 0; total_render = 0; total_push = 0;
            last_time = millis();
        }

        vTaskDelay(1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup() — Boot sequence
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[BOOT] Smart Building Master S3 — Serial SPI Edition");

    display_init();   // LovyanGFX ILI9488 init
    data_init(g_state);
    apply_dummy_rs485_slave(ENABLE_DUMMY_RS485_SLAVE == 1);

    // UI Event Callbacks (decouples UI from app logic)
    UIEventCallbacks ui_cbs;
    ui_cbs.onWiFiConnect    = [](const char* s, const char* p) { wifi_manager_connect(s, p); };
    ui_cbs.onWiFiReconnect  = []() { wifi_manager_reconnect(); };
    ui_cbs.onWiFiScan       = []() { wifi_manager_scan_request(); };
    ui_cbs.onLANSave        = []() { lan_manager_save_config(); };
    ui_cbs.onPriorityChange = [](int p) { wifi_manager_set_power(p == 0); };
    ui_cbs.onRS485Pairing   = []() { rs485_request_pairing(); };
    ui_cbs.onRS485PairingCancel = []() { rs485_cancel_pairing(); };
    ui_cbs.onRS485PairingAssign = [](uint8_t address) { rs485_request_assign_pairing_candidate(address); };
    ui_cbs.onRS485PollToggle = [](bool enabled) { rs485_set_poll_enabled(enabled); };
    ui_cbs.onRS485Test = [](uint8_t address, uint8_t cmd, bool write_command) {
        rs485_request_test(address, cmd, write_command);
    };

    screens_init(ui_cbs);  // Also calls widgets_init() which creates LGFX sprites
    touch_init();          // FT6236U I2C init + CTP_RST toggle

    rs485_task_init();     // Task_RS485 pinned to Core 0

    xTaskCreatePinnedToCore(Task_Net,   "Task_Net",   8192,  NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(Task_Touch, "Task_Touch", 4096,  NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(Task_UI,    "Task_UI",    16384, NULL, 4, NULL, 1);
}

void loop() {
    vTaskDelete(NULL); // loop() task killed — all work is in RTOS tasks
}
