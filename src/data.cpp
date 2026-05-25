#include "data.h"
#include "mapping_manager.h"
#include <Preferences.h>
#include <string.h>

BuildingState g_state;

static const char* RS485_PREF_NS = "rs485cfg";
static const uint8_t RS485_PREF_ASSIGN_SCHEMA = 2;

void data_init(BuildingState& state) {
    state.mutex = xSemaphoreCreateMutex();
    state.use_dummy = false;
    state.ui_needs_update = true;
    state.last_data_ts = millis();
    data_load_dummy(state);
    data_load_rs485_config(state);
}

void data_load_dummy(BuildingState& state) {
    if (xSemaphoreTake(state.mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        state.sensor.temp[0] = -100.0f;
        state.sensor.temp[1] = -100.0f;
        state.sensor.temp[2] = -100.0f;
        state.sensor.temp[3] = -100.0f;
        state.sensor.temp_target = 23.0f;
        state.sensor.lux = -1.0f;
        state.sensor.co2 = -1;
        state.sensor.ac_on = true;
        state.sensor.projector_on = false;
        state.sensor.light_on = true;
        state.sensor.human_presence = true;
        memset(state.sensor.sensor_error, 0, sizeof(state.sensor.sensor_error));
        state.sensor.slave_count = 2;
        state.sensor.slave_online[0] = true;
        state.sensor.slave_online[1] = true;

        state.net.wifi_connected = true;
        state.net.lan_connected  = false;
        state.net.lan_initialized = false;
        state.net.lan_dhcp_ok = false;
        state.net.lan_static_fallback = false;
        state.net.lan_checking = false;
        state.net.firebase_ok    = true;
        state.net.mqtt_ok        = false;
        state.net.net_priority   = 0;
        state.net.lan_use_dhcp   = true;
        state.dashboard_page = 0;

        strcpy(state.net.time_str,   "--:--");
        strcpy(state.net.room_name,  "Meeting Room A");
        strcpy(state.net.slave_name[0], "Slave 1");
        strcpy(state.net.slave_name[1], "Slave 2");
        strcpy(state.net.conn_status,        "Initializing...");
        strcpy(state.net.lan_status_detail,  "Not initialized");
        strcpy(state.net.wifi_status_detail, "Not connected");
        strcpy(state.net.wifi_scan_status,   "Tap REFRESH to scan");
        strcpy(state.net.lan_ip, "-");
        state.net.time_synced = false;
        state.net.time_syncing = false;
        strcpy(state.net.time_source, "-");
        strcpy(state.net.time_status, "Time not synced");
        strcpy(state.net.lan_current_gateway, "-");
        strcpy(state.net.lan_current_subnet, "-");
        strcpy(state.net.lan_current_dns, "-");
        strcpy(state.net.lan_link_status, "Unknown");
        strcpy(state.net.lan_static_ip, "192.168.1.177");
        strcpy(state.net.lan_gateway,   "192.168.1.1");
        strcpy(state.net.lan_subnet,    "255.255.255.0");
        strcpy(state.net.lan_dns,       "8.8.8.8");
        strcpy(state.net.connected_wifi_ssid, "-");

        state.net.wifi_scan_requested = false;
        state.net.wifi_scan_active = false;
        state.net.wifi_scan_start_pending = false;
        state.net.wifi_scan_radio_warming = false;
        state.net.wifi_scan_done = false;
        state.net.wifi_scan_error = false;
        state.net.wifi_scan_has_results = false;
        state.net.wifi_scan_count = 0;
        state.net.wifi_scan_requested_ts = 0;
        state.net.wifi_scan_started_ts = 0;
        state.net.wifi_scan_finished_ts = 0;
        state.net.wifi_scan_start_attempts = 0;
        memset(state.net.wifi_scan_results, 0, sizeof(state.net.wifi_scan_results));

        state.rs485.initialized = false;
        state.rs485.bus_ok = false;
        state.rs485.pairing_requested = false;
        state.rs485.pairing_active = false;
        state.rs485.pairing_candidate_ready = false;
        state.rs485.pairing_assign_requested = false;
        state.rs485.poll_enabled = true;
        state.rs485.pairing_assign_address = 0;
        state.rs485.pairing_started_ms = 0;
        state.rs485.pairing_timeout_ms = 0;
        state.rs485.pairing_timeouts = 0;
        state.rs485.slave_count = 1;
        state.rs485.packets_tx = 0;
        state.rs485.packets_rx = 0;
        state.rs485.crc_errors = 0;
        state.rs485.timeout_errors = 0;
        state.rs485.test_requested = false;
        state.rs485.test_busy = false;
        state.rs485.test_write = false;
        state.rs485.test_ok = false;
        state.rs485.test_address = 0x00;
        state.rs485.test_cmd = 0x03;
        state.rs485.test_result = 0;
        state.rs485.test_seq = 0;
        state.rs485.test_attempts = 0;
        state.rs485.test_started_ms = 0;
        state.rs485.test_done_ms = 0;
        strcpy(state.rs485.status, "RS485 not initialized");
        strcpy(state.rs485.test_status, "No manual test yet");
        memset(&state.rs485.pairing_candidate, 0, sizeof(state.rs485.pairing_candidate));
        memset(state.rs485.slaves, 0, sizeof(state.rs485.slaves));
        state.rs485.slaves[0].address = 0x00;
        strcpy(state.rs485.slaves[0].name, "Device 0");
        state.rs485.slaves[0].role = 0x00;
        state.rs485.slaves[0].enabled_mask = 0;
        state.rs485.slaves[0].temp_available_mask = 0;
        state.rs485.slaves[0].temp_enabled_mask = 0;

        for (uint8_t i = 0; i < DASHBOARD_LOGICAL_SLOT_COUNT; i++) {
            state.rs485.mappings[i].logical_id = i;
            state.rs485.mappings[i].capability_type = 0;
            state.rs485.mappings[i].slave_uid = 0;
            state.rs485.mappings[i].slave_addr = 0;
            state.rs485.mappings[i].channel = 0;
            state.rs485.mappings[i].assigned = false;
            state.rs485.mappings[i].manual_override = false;
        }
        state.rs485.mappings[LOGICAL_TEMP_SLOT_1].capability_type = CAP_TEMP;
        state.rs485.mappings[LOGICAL_TEMP_SLOT_2].capability_type = CAP_TEMP;
        state.rs485.mappings[LOGICAL_TEMP_SLOT_3].capability_type = CAP_TEMP;
        state.rs485.mappings[LOGICAL_TEMP_SLOT_4].capability_type = CAP_TEMP;
        state.rs485.mappings[LOGICAL_CO2_MAIN].capability_type = CAP_CO2;
        state.rs485.mappings[LOGICAL_LUX_MAIN].capability_type = CAP_LUX;
        state.rs485.mappings[LOGICAL_HUMAN_PRESENCE_MAIN].capability_type = CAP_HUMAN_PRESENCE;
        state.rs485.mappings[LOGICAL_AC_CONTROL].capability_type = CAP_AC_IR;
        state.rs485.mappings[LOGICAL_PROJECTOR_CONTROL].capability_type = CAP_PROJECTOR_IR;

        for (uint8_t i = 0; i < DASHBOARD_TEMP_SLOTS; i++) {
            state.rs485.dashboard.temp[i] = -100.0f;
            state.rs485.dashboard.temp_valid[i] = false;
        }
        state.rs485.dashboard.co2 = -1;
        state.rs485.dashboard.co2_valid = false;
        state.rs485.dashboard.lux = -1.0f;
        state.rs485.dashboard.lux_valid = false;
        state.rs485.dashboard.human_presence = false;
        state.rs485.dashboard.human_presence_valid = false;
        state.rs485.dashboard.ac_available = false;
        state.rs485.dashboard.projector_available = false;

        xSemaphoreGive(state.mutex);
    }
}

void data_load_rs485_config(BuildingState& state) {
    Preferences prefs;
    if (!prefs.begin(RS485_PREF_NS, true)) return;
    uint8_t assign_schema = prefs.getUChar("assign_schema", 0);

    data_lock(state);

    uint8_t slave_count = prefs.getUChar("slave_count", state.rs485.slave_count);
    if (slave_count > RS485_MAX_SLAVES) slave_count = RS485_MAX_SLAVES;
    if (slave_count == 0) slave_count = 1;
    state.rs485.slave_count = slave_count;

    for (uint8_t i = 0; i < RS485_MAX_SLAVES; i++) {
        char key[20];
        snprintf(key, sizeof(key), "s%u_addr", i);
        state.rs485.slaves[i].address = prefs.getUChar(key, state.rs485.slaves[i].address);
        snprintf(key, sizeof(key), "s%u_uid", i);
        state.rs485.slaves[i].uid = prefs.getULong(key, state.rs485.slaves[i].uid);
        snprintf(key, sizeof(key), "s%u_mac", i);
        state.rs485.slaves[i].mac = prefs.getULong64(key, state.rs485.slaves[i].mac);
        snprintf(key, sizeof(key), "s%u_cap", i);
        state.rs485.slaves[i].capability = prefs.getUShort(key, state.rs485.slaves[i].capability);
        snprintf(key, sizeof(key), "s%u_en", i);
        state.rs485.slaves[i].enabled_mask = prefs.getUShort(key, state.rs485.slaves[i].enabled_mask);
        if (assign_schema < RS485_PREF_ASSIGN_SCHEMA) {
            state.rs485.slaves[i].enabled_mask = 0;
        }
        snprintf(key, sizeof(key), "s%u_tc", i);
        state.rs485.slaves[i].temp_count = prefs.getUChar(key, state.rs485.slaves[i].temp_count);
        snprintf(key, sizeof(key), "s%u_tam", i);
        state.rs485.slaves[i].temp_available_mask = prefs.getUChar(key, state.rs485.slaves[i].temp_available_mask);
        if (state.rs485.slaves[i].temp_available_mask == 0 && state.rs485.slaves[i].temp_count > 0) {
            uint8_t mask = 0;
            uint8_t count = state.rs485.slaves[i].temp_count;
            if (count > DASHBOARD_TEMP_SLOTS) count = DASHBOARD_TEMP_SLOTS;
            for (uint8_t ch = 0; ch < count; ch++) mask |= (1 << ch);
            state.rs485.slaves[i].temp_available_mask = mask;
        }
        snprintf(key, sizeof(key), "s%u_tem", i);
        state.rs485.slaves[i].temp_enabled_mask = prefs.getUChar(key, state.rs485.slaves[i].temp_enabled_mask);
        if (assign_schema < RS485_PREF_ASSIGN_SCHEMA) {
            state.rs485.slaves[i].temp_enabled_mask = 0;
        }
        if (state.rs485.slaves[i].temp_available_mask == 0 && state.rs485.slaves[i].temp_enabled_mask != 0) {
            state.rs485.slaves[i].temp_available_mask = state.rs485.slaves[i].temp_enabled_mask & 0x0F;
            state.rs485.slaves[i].temp_count = 0;
            for (uint8_t ch = 0; ch < DASHBOARD_TEMP_SLOTS; ch++) {
                if (state.rs485.slaves[i].temp_available_mask & (1 << ch)) state.rs485.slaves[i].temp_count++;
            }
        }
        snprintf(key, sizeof(key), "s%u_cc", i);
        state.rs485.slaves[i].co2_count = prefs.getUChar(key, state.rs485.slaves[i].co2_count);
        snprintf(key, sizeof(key), "s%u_pc", i);
        state.rs485.slaves[i].presence_count = prefs.getUChar(key, state.rs485.slaves[i].presence_count);
        snprintf(key, sizeof(key), "s%u_lxc", i);
        state.rs485.slaves[i].lux_count = prefs.getUChar(key, state.rs485.slaves[i].lux_count);
        snprintf(key, sizeof(key), "s%u_rc", i);
        state.rs485.slaves[i].relay_count = prefs.getUChar(key, state.rs485.slaves[i].relay_count);
        snprintf(key, sizeof(key), "s%u_ic", i);
        state.rs485.slaves[i].ir_count = prefs.getUChar(key, state.rs485.slaves[i].ir_count);
        snprintf(key, sizeof(key), "s%u_lcdc", i);
        state.rs485.slaves[i].lcd_count = prefs.getUChar(key, state.rs485.slaves[i].lcd_count);
        snprintf(key, sizeof(key), "s%u_name", i);
        prefs.getString(key, state.rs485.slaves[i].name, sizeof(state.rs485.slaves[i].name));
        if (state.rs485.slaves[i].name[0] == '\0' && state.rs485.slaves[i].address != 0) {
            snprintf(state.rs485.slaves[i].name, sizeof(state.rs485.slaves[i].name), "Node %02X", state.rs485.slaves[i].address);
        }
    }

    for (uint8_t i = 0; i < DASHBOARD_LOGICAL_SLOT_COUNT; i++) {
        char key[20];
        snprintf(key, sizeof(key), "m%u_uid", i);
        state.rs485.mappings[i].slave_uid = prefs.getULong(key, state.rs485.mappings[i].slave_uid);
        snprintf(key, sizeof(key), "m%u_addr", i);
        state.rs485.mappings[i].slave_addr = prefs.getUChar(key, state.rs485.mappings[i].slave_addr);
        snprintf(key, sizeof(key), "m%u_ch", i);
        state.rs485.mappings[i].channel = prefs.getUChar(key, state.rs485.mappings[i].channel);
        snprintf(key, sizeof(key), "m%u_asg", i);
        state.rs485.mappings[i].assigned = prefs.getBool(key, state.rs485.mappings[i].assigned);
        snprintf(key, sizeof(key), "m%u_man", i);
        state.rs485.mappings[i].manual_override = prefs.getBool(key, state.rs485.mappings[i].manual_override);
    }

    mapping_manager_update_locked(state);
    state.ui_needs_update = true;
    data_unlock(state);

    prefs.end();
}

void data_save_rs485_config(BuildingState& state) {
    Preferences prefs;
    if (!prefs.begin(RS485_PREF_NS, false)) return;

    data_lock(state);
    prefs.putUChar("assign_schema", RS485_PREF_ASSIGN_SCHEMA);
    prefs.putUChar("slave_count", state.rs485.slave_count);

    for (uint8_t i = 0; i < RS485_MAX_SLAVES; i++) {
        char key[20];
        const RS485SlaveState& slave = state.rs485.slaves[i];
        snprintf(key, sizeof(key), "s%u_addr", i);
        prefs.putUChar(key, slave.address);
        snprintf(key, sizeof(key), "s%u_uid", i);
        prefs.putULong(key, slave.uid);
        snprintf(key, sizeof(key), "s%u_mac", i);
        prefs.putULong64(key, slave.mac);
        snprintf(key, sizeof(key), "s%u_cap", i);
        prefs.putUShort(key, slave.capability);
        snprintf(key, sizeof(key), "s%u_en", i);
        prefs.putUShort(key, slave.enabled_mask);
        snprintf(key, sizeof(key), "s%u_tc", i);
        prefs.putUChar(key, slave.temp_count);
        snprintf(key, sizeof(key), "s%u_tam", i);
        prefs.putUChar(key, slave.temp_available_mask);
        snprintf(key, sizeof(key), "s%u_tem", i);
        prefs.putUChar(key, slave.temp_enabled_mask);
        snprintf(key, sizeof(key), "s%u_cc", i);
        prefs.putUChar(key, slave.co2_count);
        snprintf(key, sizeof(key), "s%u_pc", i);
        prefs.putUChar(key, slave.presence_count);
        snprintf(key, sizeof(key), "s%u_lxc", i);
        prefs.putUChar(key, slave.lux_count);
        snprintf(key, sizeof(key), "s%u_rc", i);
        prefs.putUChar(key, slave.relay_count);
        snprintf(key, sizeof(key), "s%u_ic", i);
        prefs.putUChar(key, slave.ir_count);
        snprintf(key, sizeof(key), "s%u_lcdc", i);
        prefs.putUChar(key, slave.lcd_count);
        snprintf(key, sizeof(key), "s%u_name", i);
        prefs.putString(key, slave.name);
    }

    for (uint8_t i = 0; i < DASHBOARD_LOGICAL_SLOT_COUNT; i++) {
        char key[20];
        const LogicalMapping& mapping = state.rs485.mappings[i];
        snprintf(key, sizeof(key), "m%u_uid", i);
        prefs.putULong(key, mapping.slave_uid);
        snprintf(key, sizeof(key), "m%u_addr", i);
        prefs.putUChar(key, mapping.slave_addr);
        snprintf(key, sizeof(key), "m%u_ch", i);
        prefs.putUChar(key, mapping.channel);
        snprintf(key, sizeof(key), "m%u_asg", i);
        prefs.putBool(key, mapping.assigned);
        snprintf(key, sizeof(key), "m%u_man", i);
        prefs.putBool(key, mapping.manual_override);
    }
    data_unlock(state);

    prefs.end();
}

void data_lock(BuildingState& state) {
    xSemaphoreTake(state.mutex, portMAX_DELAY);
}

void data_unlock(BuildingState& state) {
    xSemaphoreGive(state.mutex);
}
