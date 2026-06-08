#include "mqtt_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include "data.h"
#include "rs485_manager.h"

#if __has_include("mqtt_secrets.h")
#include "mqtt_secrets.h"
#endif
#include "mqtt_defaults.h"

// const char* mqtt_server       = MQTT_SERVER_DEFAULT;
const int   mqtt_port_secure  = MQTT_PORT_SECURE_DEFAULT;
const int   mqtt_port_normal  = MQTT_PORT_NORMAL_DEFAULT;
const char* mqtt_topic_sub    = MQTT_TOPIC_SUB_DEFAULT;
const char* mqtt_topic_pub    = MQTT_TOPIC_PUB_DEFAULT;
const char* mqtt_device_name  = MQTT_DEVICE_NAME_DEFAULT;
const char* mqtt_fw_version   = MQTT_FW_VERSION_DEFAULT;
const uint32_t mqtt_publish_interval_ms = MQTT_PUBLISH_INTERVAL_MS;

WiFiClientSecure secureClient;
WiFiClient       wifiClient;
EthernetClient   ethClient;
PubSubClient     mqttClient;

static char mqtt_publish_payload[4096];

static void mqtt_set_connected_state(bool connected) {
    data_lock(g_state);
    bool changed = g_state.net.mqtt_ok != connected;
    g_state.net.mqtt_ok = connected;
    if (changed) g_state.ui_needs_update = true;
    data_unlock(g_state);
}

enum MqttTopicKind : uint8_t {
    MQTT_TOPIC_TEMP,
    MQTT_TOPIC_CO2,
    MQTT_TOPIC_LUX,
    MQTT_TOPIC_HUMAN,
    MQTT_TOPIC_LED,
    MQTT_TOPIC_AC,
    MQTT_TOPIC_PROJECTOR,
    MQTT_TOPIC_ALERT,
    MQTT_TOPIC_ACTIVE,
    MQTT_TOPIC_MASTER,
    MQTT_TOPIC_SCHEDULE
};

static const char* mqtt_class_name_locked() {
    return g_state.net.class_name[0] ? g_state.net.class_name : "HD01";
}

static const char* mqtt_device_name_locked() {
    return g_state.net.device_name[0] ? g_state.net.device_name : mqtt_device_name;
}

static void mqtt_build_topic_locked(MqttTopicKind kind, char* out, size_t out_len) {
    const char* suffix = "status";
    switch (kind) {
        case MQTT_TOPIC_TEMP: suffix = "temp"; break;
        case MQTT_TOPIC_CO2: suffix = "co2"; break;
        case MQTT_TOPIC_LUX: suffix = "lux"; break;
        case MQTT_TOPIC_HUMAN: suffix = "human"; break;
        case MQTT_TOPIC_LED: suffix = "led"; break;
        case MQTT_TOPIC_AC: suffix = "ac"; break;
        case MQTT_TOPIC_PROJECTOR: suffix = "projector"; break;
        case MQTT_TOPIC_ALERT: suffix = "alert"; break;
        case MQTT_TOPIC_ACTIVE: suffix = "active"; break;
        case MQTT_TOPIC_MASTER: suffix = "master"; break;
        case MQTT_TOPIC_SCHEDULE: suffix = "schedule"; break;
    }
    snprintf(out, out_len, "%s/%s/%s",
             mqtt_class_name_locked(),
             "data",
             suffix);
}

static void mqtt_build_control_topic_locked(MqttTopicKind kind, char* out, size_t out_len) {
    const char* suffix = "status";
    switch (kind) {
        case MQTT_TOPIC_LED: suffix = "led"; break;
        case MQTT_TOPIC_AC: suffix = "ac"; break;
        case MQTT_TOPIC_PROJECTOR: suffix = "projector"; break;
        case MQTT_TOPIC_SCHEDULE: suffix = "schedule"; break;
        default: break;
    }
    snprintf(out, out_len, "%s/control/%s", mqtt_class_name_locked(), suffix);
}

static bool mqtt_publish_raw(const char* topic, const char* payload, bool retained, const char* label) {
    bool ok = mqttClient.publish(topic, payload, retained);
    Serial.printf("[MQTT] Publish %s %s topic=%s payload=%s\n",
                  label,
                  ok ? "OK" : "FAIL",
                  topic,
                  payload);
    return ok;
}

static bool mqtt_publish_json(const char* topic, JsonDocument& doc, bool retained, const char* label) {
    size_t payload_len = serializeJson(doc, mqtt_publish_payload, sizeof(mqtt_publish_payload));
    if (payload_len == 0 || payload_len >= sizeof(mqtt_publish_payload)) {
        Serial.printf("[MQTT] Publish skipped: %s payload too large\n", label);
        return false;
    }
    bool ok = mqttClient.publish(topic, reinterpret_cast<const uint8_t*>(mqtt_publish_payload), payload_len, retained);
    Serial.printf("[MQTT] Publish %s %s topic=%s bytes=%u retain=%s\n",
                  label,
                  ok ? "OK" : "FAIL",
                  topic,
                  (unsigned)payload_len,
                  retained ? "true" : "false");
    return ok;
}

static bool mqtt_parse_bool_payload(const byte* payload, unsigned int length, bool& value) {
    if (!payload || length == 0) return false;

    char buf[12];
    unsigned int copy_len = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';

    char* start = buf;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    char* end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }

    if (strcmp(start, "1") == 0 || strcasecmp(start, "on") == 0 || strcasecmp(start, "true") == 0) {
        value = true;
        return true;
    }
    if (strcmp(start, "0") == 0 || strcasecmp(start, "off") == 0 || strcasecmp(start, "false") == 0) {
        value = false;
        return true;
    }
    return false;
}

static float mqtt_clamp_ac_target(float target_c) {
    if (target_c < 16.0f) return 16.0f;
    if (target_c > 30.0f) return 30.0f;
    return target_c;
}

static uint8_t mqtt_clamp_ac_target_u8(float target_c) {
    return (uint8_t)(mqtt_clamp_ac_target(target_c) + 0.5f);
}

static uint8_t mqtt_normalize_ac_enum(uint8_t value) {
    return value <= 99 ? value : 99;
}

static void mqtt_format_ac_payload(bool power, float target_c, uint8_t fan_speed, uint8_t swing_mode,
                                   char* out, size_t out_len) {
    snprintf(out, out_len, "%02u%02u%02u%02u",
             power ? 1 : 0,
             mqtt_clamp_ac_target_u8(target_c),
             mqtt_normalize_ac_enum(fan_speed),
             mqtt_normalize_ac_enum(swing_mode));
}

static bool mqtt_parse_ac_payload(const byte* payload, unsigned int length,
                                  bool& power, float& target_c,
                                  uint8_t& fan_speed, uint8_t& swing_mode) {
    if (!payload || length == 0) return false;

    char buf[16];
    unsigned int copy_len = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';

    char* start = buf;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    char* end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }

    if (strlen(start) != 8) return false;
    for (uint8_t i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)start[i])) return false;
    }

    uint8_t pp = (start[0] - '0') * 10 + (start[1] - '0');
    uint8_t tt = (start[2] - '0') * 10 + (start[3] - '0');
    uint8_t ff = (start[4] - '0') * 10 + (start[5] - '0');
    uint8_t ss = (start[6] - '0') * 10 + (start[7] - '0');
    if (pp > 1) return false;

    power = pp == 1;
    target_c = mqtt_clamp_ac_target((float)tt);
    fan_speed = mqtt_normalize_ac_enum(ff);
    swing_mode = mqtt_normalize_ac_enum(ss);
    return true;
}

static void mqtt_add_capability_name(JsonArray arr, uint16_t mask, uint16_t capability, const char* name) {
    if (mask & capability) arr.add(name);
}

static void mqtt_format_mac(uint64_t mac, char* out, size_t out_len) {
    snprintf(out, out_len,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)((mac >> 40) & 0xFF),
             (uint8_t)((mac >> 32) & 0xFF),
             (uint8_t)((mac >> 24) & 0xFF),
             (uint8_t)((mac >> 16) & 0xFF),
             (uint8_t)((mac >> 8) & 0xFF),
             (uint8_t)(mac & 0xFF));
}

static DeviceRegistryStatus mqtt_slave_status(const RS485SlaveState& slave) {
    if (slave.degraded) return DEVICE_STATUS_DEGRADED;
    if (slave.online) return DEVICE_STATUS_ONLINE;
    if (slave.address != 0 || slave.mac != 0 || slave.uid != 0) return DEVICE_STATUS_OFFLINE;
    return DEVICE_STATUS_UNKNOWN;
}

static bool mqtt_pairing_candidate_unknown_locked() {
    if (!g_state.rs485.pairing_candidate_ready) return false;
    const RS485SlaveState& candidate = g_state.rs485.pairing_candidate;
    if (candidate.mac == 0 && candidate.uid == 0) return true;

    uint8_t count = g_state.rs485.slave_count;
    if (count > RS485_MAX_SLAVES) count = RS485_MAX_SLAVES;
    for (uint8_t i = 0; i < count; i++) {
        const RS485SlaveState& slave = g_state.rs485.slaves[i];
        if (candidate.mac != 0 && slave.mac == candidate.mac) return false;
        if (candidate.mac == 0 && candidate.uid != 0 && slave.uid == candidate.uid) return false;
    }
    return true;
}

static bool mqtt_has_ir_combo_locked() {
    uint8_t count = g_state.rs485.slave_count;
    if (count > RS485_MAX_SLAVES) count = RS485_MAX_SLAVES;
    for (uint8_t i = 0; i < count; i++) {
        const RS485SlaveState& slave = g_state.rs485.slaves[i];
        if (slave.profile == IR_COMBO_NODE &&
            (slave.enabled_mask & CAP_AC_IR) &&
            (slave.enabled_mask & CAP_PROJECTOR_IR)) {
            return true;
        }
    }
    return false;
}

static void mqtt_publish_legacy_state() {
    JsonDocument doc;
    doc["type"] = "smart_building_master_state";
    doc["device_name"] = mqtt_device_name_locked();
    doc["firmware_version"] = mqtt_fw_version;
    doc["publisher"] = "Hansel Kay";
    doc["lab"] = "CE LAB";
    doc["timestamp_ms"] = millis();

    data_lock(g_state);

    JsonObject network = doc["network"].to<JsonObject>();
    network["priority"] = g_state.net.net_priority == 1 ? "lan" : "wifi";
    network["wifi_connected"] = g_state.net.wifi_connected;
    network["lan_connected"] = g_state.net.lan_connected;
    network["mqtt_connected"] = mqttClient.connected();

    JsonArray slaves = doc["slaves"].to<JsonArray>();
    uint8_t slave_count = g_state.rs485.slave_count;
    if (slave_count > RS485_MAX_SLAVES) slave_count = RS485_MAX_SLAVES;
    for (uint8_t i = 0; i < slave_count; i++) {
        const RS485SlaveState& slave = g_state.rs485.slaves[i];
        if (slave.address == 0 && slave.uid == 0 && slave.mac == 0 && !slave.online) continue;

        JsonObject item = slaves.add<JsonObject>();
        item["address"] = slave.address;

        char uid_buf[12];
        snprintf(uid_buf, sizeof(uid_buf), "%08lX", (unsigned long)slave.uid);
        item["uid"] = uid_buf;

        char mac_buf[18];
        mqtt_format_mac(slave.mac, mac_buf, sizeof(mac_buf));
        item["mac"] = mac_buf;
        item["name"] = slave.name[0] ? slave.name : "Node";
        item["room"] = slave.room[0] ? slave.room : mqtt_class_name_locked();
        item["profile"] = device_profile_name(slave.profile);
        item["status"] = device_registry_status_name(mqtt_slave_status(slave));
        if (slave.last_seen != 0) item["last_seen_ms"] = slave.last_seen;
        else item["last_seen_ms"].set(nullptr);
        item["online"] = slave.online;

        JsonArray capability = item["capability"].to<JsonArray>();
        mqtt_add_capability_name(capability, slave.capability, CAP_TEMP, "temp");
        mqtt_add_capability_name(capability, slave.capability, CAP_CO2, "co2");
        mqtt_add_capability_name(capability, slave.capability, CAP_HUMAN_PRESENCE, "presence");
        mqtt_add_capability_name(capability, slave.capability, CAP_AC_IR, "ac");
        mqtt_add_capability_name(capability, slave.capability, CAP_PROJECTOR_IR, "projector");
        mqtt_add_capability_name(capability, slave.capability, CAP_LIGHT_RELAY, "relay");
        mqtt_add_capability_name(capability, slave.capability, CAP_LUX, "lux");

        JsonArray enabled = item["enabled"].to<JsonArray>();
        mqtt_add_capability_name(enabled, slave.enabled_mask, CAP_TEMP, "temp");
        mqtt_add_capability_name(enabled, slave.enabled_mask, CAP_CO2, "co2");
        mqtt_add_capability_name(enabled, slave.enabled_mask, CAP_HUMAN_PRESENCE, "presence");
        mqtt_add_capability_name(enabled, slave.enabled_mask, CAP_AC_IR, "ac");
        mqtt_add_capability_name(enabled, slave.enabled_mask, CAP_PROJECTOR_IR, "projector");
        mqtt_add_capability_name(enabled, slave.enabled_mask, CAP_LIGHT_RELAY, "relay");
        mqtt_add_capability_name(enabled, slave.enabled_mask, CAP_LUX, "lux");

        item["relay_count"] = slave.relay_count;
    }

    JsonObject data = doc["data"].to<JsonObject>();
    JsonObject temp = data["temperature"].to<JsonObject>();
    JsonArray points = temp["points_c"].to<JsonArray>();
    float sum = 0.0f;
    uint8_t valid_temp = 0;
    for (uint8_t i = 0; i < DASHBOARD_TEMP_SLOTS; i++) {
        if (g_state.rs485.dashboard.temp_valid[i]) {
            float value = g_state.rs485.dashboard.temp[i];
            points.add(value);
            sum += value;
            valid_temp++;
        } else {
            points.add(nullptr);
        }
    }
    if (valid_temp > 0) temp["avg_c"] = sum / valid_temp;
    else temp["avg_c"].set(nullptr);

    if (g_state.rs485.dashboard.co2_valid) data["co2_ppm"] = g_state.rs485.dashboard.co2;
    else data["co2_ppm"].set(nullptr);

    if (g_state.rs485.dashboard.lux_valid) data["lux"] = g_state.rs485.dashboard.lux;
    else data["lux"].set(nullptr);

    if (g_state.rs485.dashboard.human_presence_valid) data["human_presence"] = g_state.rs485.dashboard.human_presence;
    else data["human_presence"].set(nullptr);

    JsonObject controls = doc["controls"].to<JsonObject>();
    JsonObject ac = controls["ac"].to<JsonObject>();
    ac["available"] = g_state.rs485.dashboard.ac_available;
    ac["power"] = g_state.sensor.ac_on;
    ac["target_c"] = g_state.sensor.temp_target;

    JsonObject projector = controls["projector"].to<JsonObject>();
    projector["available"] = g_state.rs485.dashboard.projector_available;
    projector["power"] = g_state.sensor.projector_on;

    JsonObject lights = controls["lights"].to<JsonObject>();
    JsonArray channels = lights["channels"].to<JsonArray>();
    uint8_t light_id = 1;
    for (uint8_t i = 0; i < slave_count && light_id <= 4; i++) {
        const RS485SlaveState& slave = g_state.rs485.slaves[i];
        if (!slave.online || !(slave.enabled_mask & CAP_LIGHT_RELAY)) continue;

        uint8_t relay_count = slave.relay_count;
        if (relay_count == 0 && (slave.capability & CAP_LIGHT_RELAY)) relay_count = 1;
        if (relay_count > 2) relay_count = 2;
        for (uint8_t relay = 0; relay < relay_count && light_id <= 4; relay++) {
            JsonObject channel = channels.add<JsonObject>();
            channel["id"] = light_id;
            char name_buf[12];
            snprintf(name_buf, sizeof(name_buf), "Light %u", light_id);
            channel["name"] = name_buf;
            channel["power"] = slave.relay_state[relay] != 0;
            light_id++;
        }
    }
    lights["available"] = light_id > 1;

    size_t payload_len = serializeJson(doc, mqtt_publish_payload, sizeof(mqtt_publish_payload));

    data_unlock(g_state);

    if (payload_len == 0 || payload_len >= sizeof(mqtt_publish_payload)) {
        Serial.println("[MQTT] Publish skipped: state payload too large");
        return;
    }

    bool ok = mqttClient.publish(mqtt_topic_pub, reinterpret_cast<const uint8_t*>(mqtt_publish_payload), payload_len, true);
    Serial.printf("[MQTT] Publish %s topic=%s bytes=%u\n",
                  ok ? "OK" : "FAIL",
                  mqtt_topic_pub,
                  (unsigned)payload_len);
}

static void mqtt_publish_v2_state() {
    char topic_temp[64];
    char topic_co2[48];
    char topic_lux[48];
    char topic_human[48];
    char topic_led[48];
    char topic_ac[48];
    char topic_projector[48];
    char topic_alert[64];
    char topic_active[64];
    char temp_payload[16];
    char co2_payload[16];
    char lux_payload[16];
    char human_payload[8];
    char led_payload[8];
    char ac_payload[12];
    char projector_payload[8];
    char alert_payload[8];

    data_lock(g_state);

    mqtt_build_topic_locked(MQTT_TOPIC_TEMP, topic_temp, sizeof(topic_temp));
    mqtt_build_topic_locked(MQTT_TOPIC_CO2, topic_co2, sizeof(topic_co2));
    mqtt_build_topic_locked(MQTT_TOPIC_LUX, topic_lux, sizeof(topic_lux));
    mqtt_build_topic_locked(MQTT_TOPIC_HUMAN, topic_human, sizeof(topic_human));
    mqtt_build_topic_locked(MQTT_TOPIC_LED, topic_led, sizeof(topic_led));
    mqtt_build_topic_locked(MQTT_TOPIC_AC, topic_ac, sizeof(topic_ac));
    mqtt_build_topic_locked(MQTT_TOPIC_PROJECTOR, topic_projector, sizeof(topic_projector));
    mqtt_build_topic_locked(MQTT_TOPIC_ALERT, topic_alert, sizeof(topic_alert));
    mqtt_build_topic_locked(MQTT_TOPIC_ACTIVE, topic_active, sizeof(topic_active));

    float sum = 0.0f;
    uint8_t valid_temp = 0;
    for (uint8_t i = 0; i < DASHBOARD_TEMP_SLOTS; i++) {
        if (g_state.rs485.dashboard.temp_valid[i]) {
            sum += g_state.rs485.dashboard.temp[i];
            valid_temp++;
        }
    }
    if (valid_temp > 0) snprintf(temp_payload, sizeof(temp_payload), "%d", (int)((sum / valid_temp) + 0.5f));
    else snprintf(temp_payload, sizeof(temp_payload), "-1");

    if (g_state.rs485.dashboard.co2_valid) snprintf(co2_payload, sizeof(co2_payload), "%d", g_state.rs485.dashboard.co2);
    else snprintf(co2_payload, sizeof(co2_payload), "-1");

    if (g_state.rs485.dashboard.lux_valid) snprintf(lux_payload, sizeof(lux_payload), "%d", (int)g_state.rs485.dashboard.lux);
    else snprintf(lux_payload, sizeof(lux_payload), "-1");

    if (g_state.rs485.dashboard.human_presence_valid) snprintf(human_payload, sizeof(human_payload), "%u", g_state.rs485.dashboard.human_presence ? 1 : 0);
    else snprintf(human_payload, sizeof(human_payload), "-1");

    bool led_on = false;
    uint8_t light_id = 1;
    uint8_t slave_count = g_state.rs485.slave_count;
    if (slave_count > RS485_MAX_SLAVES) slave_count = RS485_MAX_SLAVES;
    for (uint8_t i = 0; i < slave_count && light_id <= 4; i++) {
        const RS485SlaveState& slave = g_state.rs485.slaves[i];
        if (!slave.online || !(slave.enabled_mask & CAP_LIGHT_RELAY)) continue;
        uint8_t relay_count = slave.relay_count;
        if (relay_count == 0 && (slave.capability & CAP_LIGHT_RELAY)) relay_count = 1;
        if (relay_count > 2) relay_count = 2;
        for (uint8_t relay = 0; relay < relay_count && light_id <= 4; relay++) {
            if (slave.relay_state[relay]) led_on = true;
            light_id++;
        }
    }
    snprintf(led_payload, sizeof(led_payload), "%u", led_on ? 1 : 0);

    mqtt_format_ac_payload(g_state.sensor.ac_on,
                           g_state.sensor.temp_target,
                           g_state.sensor.ac_fan_speed,
                           g_state.sensor.ac_swing_mode,
                           ac_payload,
                           sizeof(ac_payload));

    snprintf(projector_payload, sizeof(projector_payload), "%u", g_state.sensor.projector_on ? 1 : 0);

    uint16_t alert_mask = 0;
    if (valid_temp == 0) alert_mask |= (1 << 0);
    if (!g_state.rs485.dashboard.co2_valid) alert_mask |= (1 << 1);
    if (!g_state.rs485.dashboard.lux_valid) alert_mask |= (1 << 2);
    if (!g_state.rs485.dashboard.human_presence_valid) alert_mask |= (1 << 3);
    if (!g_state.rs485.bus_ok && light_id > 1) alert_mask |= (1 << 4);
    if (g_state.sensor.proj_hardware_failed || (!g_state.rs485.bus_ok && g_state.rs485.dashboard.projector_available)) alert_mask |= (1 << 5);
    if (!g_state.rs485.bus_ok && g_state.rs485.dashboard.ac_available) alert_mask |= (1 << 6);
    if (g_state.sensor.light_anomaly_alert) alert_mask |= (1 << 7);
    snprintf(alert_payload, sizeof(alert_payload), "%u", alert_mask);

    data_unlock(g_state);

    mqtt_publish_raw(topic_temp, temp_payload, true, "temperature");
    mqtt_publish_raw(topic_co2, co2_payload, true, "co2");
    mqtt_publish_raw(topic_lux, lux_payload, true, "lux");
    mqtt_publish_raw(topic_human, human_payload, true, "human");
    mqtt_publish_raw(topic_led, led_payload, true, "led");
    mqtt_publish_raw(topic_ac, ac_payload, true, "ac");
    mqtt_publish_raw(topic_projector, projector_payload, true, "projector");
    mqtt_publish_raw(topic_alert, alert_payload, true, "alert");
    mqtt_publish_raw(topic_active, "1", true, "active");
}

void mqtt_publish_state() {
    mqtt_publish_v2_state();
}

static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    char topic_led[48];
    char topic_ac[48];
    char topic_projector[48];
    char topic_schedule[48];
    bool presence = false;

    data_lock(g_state);
    mqtt_build_control_topic_locked(MQTT_TOPIC_LED, topic_led, sizeof(topic_led));
    mqtt_build_control_topic_locked(MQTT_TOPIC_AC, topic_ac, sizeof(topic_ac));
    mqtt_build_control_topic_locked(MQTT_TOPIC_PROJECTOR, topic_projector, sizeof(topic_projector));
    mqtt_build_control_topic_locked(MQTT_TOPIC_SCHEDULE, topic_schedule, sizeof(topic_schedule));
    presence = g_state.sensor.human_presence;
    data_unlock(g_state);

    if (strcmp(topic, topic_led) == 0) {
        if (presence) {
            Serial.println("[MQTT] LED command ignored due to active human presence");
            return;
        }

        bool scalar_on = false;
        if (mqtt_parse_bool_payload(payload, length, scalar_on)) {
            data_lock(g_state);
            g_state.sensor.light_on = scalar_on;
            g_state.ui_needs_update = true;
            data_unlock(g_state);
            rs485_request_light_command(scalar_on);
            Serial.println("[MQTT] LED scalar command applied");
            mqtt_publish_state();
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error) {
            Serial.printf("[MQTT] JSON parse failed: %s\n", error.c_str());
            return;
        }
        if (doc["positions"].is<JsonArray>() && doc["command"].isNull() && doc["power"].isNull()) {
            return;
        }

        bool changed = false;
        bool desired_on = false;
        data_lock(g_state);
        if (doc["positions"].is<JsonArray>()) {
            for (JsonObject pos : doc["positions"].as<JsonArray>()) {
                const char* state_text = pos["state"] | "";
                if (strcmp(state_text, "ON") == 0 || strcmp(state_text, "OFF") == 0) {
                    g_state.sensor.light_on = strcmp(state_text, "ON") == 0;
                    desired_on = g_state.sensor.light_on;
                    changed = true;
                }
            }
        } else if (!doc["power"].isNull()) {
            if (doc["power"].is<const char*>()) {
                const char* power = doc["power"] | "";
                g_state.sensor.light_on = strcmp(power, "ON") == 0 || strcmp(power, "on") == 0;
            } else {
                g_state.sensor.light_on = doc["power"].as<bool>();
            }
            desired_on = g_state.sensor.light_on;
            changed = true;
        }
        if (changed) g_state.ui_needs_update = true;
        data_unlock(g_state);
        if (changed) {
            rs485_request_light_command(desired_on);
            Serial.println("[MQTT] LED command applied");
            mqtt_publish_state();
        }
        return;
    }

    if (strcmp(topic, topic_ac) == 0) {
        bool desired_power = false;
        float desired_target = 0.0f;
        uint8_t desired_fan = 0;
        uint8_t desired_swing = 0;
        if (mqtt_parse_ac_payload(payload, length, desired_power, desired_target, desired_fan, desired_swing)) {
            data_lock(g_state);
            if (presence && !desired_power) {
                desired_power = true;
                Serial.println("[MQTT] AC OFF command ignored due to active human presence");
            }
            g_state.sensor.ac_on = desired_power;
            g_state.sensor.temp_target = desired_target;
            g_state.sensor.ac_fan_speed = desired_fan;
            g_state.sensor.ac_swing_mode = desired_swing;
            g_state.ui_needs_update = true;
            data_unlock(g_state);
            rs485_request_ac_command(desired_power, desired_target, 0, desired_fan, desired_swing);
            Serial.println("[MQTT] AC PPTTFFSS command applied");
            mqtt_publish_state();
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error) {
            Serial.printf("[MQTT] JSON parse failed: %s\n", error.c_str());
            return;
        }
        if (!doc["available"].isNull() && doc["power"].is<const char*>()) return;

        data_lock(g_state);
        if (!doc["power"].isNull()) {
            bool new_power = false;
            if (doc["power"].is<const char*>()) {
                const char* power = doc["power"] | "";
                new_power = strcmp(power, "ON") == 0 || strcmp(power, "on") == 0;
            } else {
                new_power = doc["power"].as<bool>();
            }
            if (presence && !new_power) {
                Serial.println("[MQTT] AC OFF JSON command ignored due to active human presence");
            } else {
                g_state.sensor.ac_on = new_power;
            }
        }
        if (!doc["target_c"].isNull()) {
            g_state.sensor.temp_target = mqtt_clamp_ac_target(doc["target_c"].as<float>());
        }
        if (!doc["fan_speed"].isNull()) {
            g_state.sensor.ac_fan_speed = mqtt_normalize_ac_enum(doc["fan_speed"].as<uint8_t>());
        }
        if (!doc["swing"].isNull()) {
            g_state.sensor.ac_swing_mode = mqtt_normalize_ac_enum(doc["swing"].as<uint8_t>());
        }
        desired_power = g_state.sensor.ac_on;
        desired_target = g_state.sensor.temp_target;
        desired_fan = g_state.sensor.ac_fan_speed;
        desired_swing = g_state.sensor.ac_swing_mode;
        g_state.ui_needs_update = true;
        data_unlock(g_state);
        rs485_request_ac_command(desired_power, desired_target, 0, desired_fan, desired_swing);
        Serial.println("[MQTT] AC command applied");
        mqtt_publish_state();
        return;
    }

    if (strcmp(topic, topic_projector) == 0) {
        bool scalar_on = false;
        if (mqtt_parse_bool_payload(payload, length, scalar_on)) {
            data_lock(g_state);
            g_state.sensor.projector_on = scalar_on;
            g_state.ui_needs_update = true;
            data_unlock(g_state);
            rs485_request_projector_command(scalar_on);
            Serial.println("[MQTT] Projector scalar command applied");
            mqtt_publish_state();
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error) {
            Serial.printf("[MQTT] JSON parse failed: %s\n", error.c_str());
            return;
        }
        if (!doc["available"].isNull() && doc["power"].is<const char*>()) return;

        bool desired_power = false;
        data_lock(g_state);
        if (!doc["power"].isNull()) {
            if (doc["power"].is<const char*>()) {
                const char* power = doc["power"] | "";
                g_state.sensor.projector_on = strcmp(power, "ON") == 0 || strcmp(power, "on") == 0;
            } else {
                g_state.sensor.projector_on = doc["power"].as<bool>();
            }
        }
        desired_power = g_state.sensor.projector_on;
        g_state.ui_needs_update = true;
        data_unlock(g_state);
        rs485_request_projector_command(desired_power);
        Serial.println("[MQTT] Projector command applied");
        mqtt_publish_state();
        return;
    }

    if (strcmp(topic, topic_schedule) == 0) {
        char payload_str[32] = {0};
        size_t len = (length < sizeof(payload_str) - 1) ? length : (sizeof(payload_str) - 1);
        memcpy(payload_str, payload, len);

        if (strcmp(payload_str, "PRE_CLASS_ON") == 0) {
            Serial.println("[MQTT] Schedule: PRE_CLASS_ON command received");
            data_lock(g_state);
            g_state.sensor.ac_on = true;
            g_state.sensor.light_on = true;
            g_state.sensor.sched_shutdown_active = false;
            g_state.sensor.sched_shutdown_timer_ms = 0;
            g_state.ui_needs_update = true;
            data_unlock(g_state);

            rs485_request_ac_command(true, g_state.sensor.temp_target, 0, g_state.sensor.ac_fan_speed, g_state.sensor.ac_swing_mode);
            rs485_request_light_command(true);
            mqtt_publish_state();
        } else if (strcmp(payload_str, "CLASS_ENDED") == 0) {
            Serial.println("[MQTT] Schedule: CLASS_ENDED command received");
            data_lock(g_state);
            g_state.sensor.sched_shutdown_active = true;
            g_state.sensor.sched_shutdown_timer_ms = millis() + (20 * 60 * 1000);
            g_state.ui_needs_update = true;
            data_unlock(g_state);
            mqtt_publish_state();
        }
        return;
    }

    if (strcmp(topic, mqtt_topic_sub) == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error) {
            Serial.printf("[MQTT] JSON parse failed: %s\n", error.c_str());
            return;
        }
        const char* type = doc["type"] | "";
        if (strcmp(type, "smart_building_master_state") == 0) {
            return;
        }

        if (strcmp(type, "smart_building_master_command") == 0) {
            JsonObject controls = doc["controls"];
            bool projector_changed = false;
            bool projector_power = false;
            bool ac_changed = false;
            bool ac_power = false;
            float ac_target = 0.0f;
            uint8_t ac_fan = 0;
            uint8_t ac_swing = 0;
            bool light_changed = false;
            bool light_power = false;
            data_lock(g_state);
            if (!controls["projector"]["power"].isNull()) {
                g_state.sensor.projector_on = controls["projector"]["power"].as<bool>();
                projector_changed = true;
                projector_power = g_state.sensor.projector_on;
            }
            if (!controls["ac"]["power"].isNull()) {
                bool new_ac_power = controls["ac"]["power"].as<bool>();
                if (presence && !new_ac_power) {
                    Serial.println("[MQTT] Master AC power OFF command ignored due to active human presence");
                } else {
                    g_state.sensor.ac_on = new_ac_power;
                    ac_changed = true;
                }
            }
            if (!controls["ac"]["target_c"].isNull()) {
                g_state.sensor.temp_target = mqtt_clamp_ac_target(controls["ac"]["target_c"].as<float>());
                ac_changed = true;
            }
            if (!controls["ac"]["fan_speed"].isNull()) {
                g_state.sensor.ac_fan_speed = mqtt_normalize_ac_enum(controls["ac"]["fan_speed"].as<uint8_t>());
                ac_changed = true;
            }
            if (!controls["ac"]["swing"].isNull()) {
                g_state.sensor.ac_swing_mode = mqtt_normalize_ac_enum(controls["ac"]["swing"].as<uint8_t>());
                ac_changed = true;
            }
            ac_power = g_state.sensor.ac_on;
            ac_target = g_state.sensor.temp_target;
            ac_fan = g_state.sensor.ac_fan_speed;
            ac_swing = g_state.sensor.ac_swing_mode;
            if (controls["lights"].is<JsonArray>()) {
                for (JsonObject light : controls["lights"].as<JsonArray>()) {
                    if (!light["power"].isNull()) {
                        if (presence) {
                            Serial.println("[MQTT] Master light power command ignored due to active human presence");
                        } else {
                            g_state.sensor.light_on = light["power"].as<bool>();
                            light_changed = true;
                            light_power = g_state.sensor.light_on;
                        }
                    }
                }
            }
            g_state.ui_needs_update = true;
            data_unlock(g_state);
            if (projector_changed) rs485_request_projector_command(projector_power);
            if (ac_changed) rs485_request_ac_command(ac_power, ac_target, 0, ac_fan, ac_swing);
        }

        float temp = doc["temperature"].isNull() ? -100.0f : doc["temperature"].as<float>();
        float lux  = doc["lux"].isNull()         ? -1.0f   : doc["lux"].as<float>();
        int   co2  = doc["co2"].isNull()          ? -1      : doc["co2"].as<int>();

        data_lock(g_state);
        if (temp > -50.0f) {
            g_state.sensor.temp[0] = temp;
            g_state.sensor.sensor_error[0] = false;
        } else {
            g_state.sensor.temp[0] = -100.0f;
        }
        g_state.sensor.lux  = lux;
        g_state.sensor.co2  = co2;
        g_state.last_data_ts     = millis();
        g_state.ui_needs_update  = true;
        data_unlock(g_state);

        Serial.printf("[MQTT] Data: T:%.1f L:%.0f C:%d\n", temp, lux, co2);
    }
}

void mqtt_init() {
    secureClient.setInsecure();
    mqttClient.setCallback(mqtt_callback);
    mqttClient.setBufferSize(4096);
}

static void mqtt_subscribe_v2_topics() {
    char topic[64];

    data_lock(g_state);
    mqtt_build_control_topic_locked(MQTT_TOPIC_LED, topic, sizeof(topic));
    data_unlock(g_state);
    mqttClient.subscribe(topic, 1);
    Serial.printf("[MQTT] Subscribe LED cmd topic=%s qos=1 retain=false\n", topic);

    data_lock(g_state);
    mqtt_build_control_topic_locked(MQTT_TOPIC_AC, topic, sizeof(topic));
    data_unlock(g_state);
    mqttClient.subscribe(topic, 1);
    Serial.printf("[MQTT] Subscribe AC cmd topic=%s qos=1 retain=false\n", topic);

    data_lock(g_state);
    mqtt_build_control_topic_locked(MQTT_TOPIC_PROJECTOR, topic, sizeof(topic));
    data_unlock(g_state);
    mqttClient.subscribe(topic, 1);
    Serial.printf("[MQTT] Subscribe Projector cmd topic=%s qos=1 retain=false\n", topic);

    data_lock(g_state);
    mqtt_build_control_topic_locked(MQTT_TOPIC_SCHEDULE, topic, sizeof(topic));
    data_unlock(g_state);
    mqttClient.subscribe(topic, 1);
    Serial.printf("[MQTT] Subscribe Schedule cmd topic=%s qos=1 retain=false\n", topic);
}

static void reconnect() {
    if (!mqttClient.connected()) {
        char server[64];
        char user[32];
        char pass[64];
        uint16_t port = 0;
        bool use_tls = false;
        char active_topic[64];
        data_lock(g_state);
        strncpy(server, g_state.net.mqtt_server, sizeof(server) - 1);
        server[sizeof(server) - 1] = '\0';
        strncpy(user, g_state.net.mqtt_user, sizeof(user) - 1);
        user[sizeof(user) - 1] = '\0';
        strncpy(pass, g_state.net.mqtt_pass, sizeof(pass) - 1);
        pass[sizeof(pass) - 1] = '\0';
        port = g_state.net.mqtt_port;
        use_tls = g_state.net.mqtt_use_tls;
        mqtt_build_topic_locked(MQTT_TOPIC_ACTIVE, active_topic, sizeof(active_topic));
        data_unlock(g_state);

        if (g_state.net.net_priority == 1 && g_state.net.lan_connected) {
            if (use_tls) mqttClient.setClient(secureClient);
            else mqttClient.setClient(ethClient);
            mqttClient.setServer(server, port);
            Serial.print(use_tls ? "[MQTT] Connecting via LAN (TLS)..." : "[MQTT] Connecting via LAN...");
        } else if (g_state.net.wifi_connected) {
            if (use_tls) mqttClient.setClient(secureClient);
            else mqttClient.setClient(wifiClient);
            mqttClient.setServer(server, port);
            Serial.print(use_tls ? "[MQTT] Connecting via WiFi (TLS)..." : "[MQTT] Connecting via WiFi...");
        } else {
            mqtt_set_connected_state(false);
            return;
        }

        Serial.printf("[MQTT] Target server=%s port=%u tls=%s user=%s\n",
                      server,
                      port,
                      use_tls ? "yes" : "no",
                      user[0] ? user : "(empty)");
        String clientId = "MasterS3-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str(), user, pass, active_topic, 1, true, "0")) {
            Serial.println("OK");
            if (mqtt_topic_pub && mqtt_topic_pub[0]) {
                bool cleared = mqttClient.publish(mqtt_topic_pub, "", true);
                Serial.printf("[MQTT] Clear legacy retained topic=%s %s\n",
                              mqtt_topic_pub,
                              cleared ? "OK" : "FAIL");
            }
            mqtt_subscribe_v2_topics();
            mqtt_set_connected_state(true);
            mqtt_publish_state();
        } else {
            Serial.printf("Failed (rc=%d)\n", mqttClient.state());
            mqtt_set_connected_state(false);
        }
    }
}

void mqtt_loop() {
    bool has_net = g_state.net.wifi_connected || g_state.net.lan_connected;
    if (!has_net) {
        mqtt_set_connected_state(false);
        return;
    }

    if (!mqttClient.connected()) {
        mqtt_set_connected_state(false);
        static uint32_t last_reconnect = 0;
        if (millis() - last_reconnect > 5000) {
            last_reconnect = millis();
            reconnect();
        }
    } else {
        mqtt_set_connected_state(true);
        mqttClient.loop();
        static uint32_t last_publish = 0;
        uint32_t now = millis();
        if (now - last_publish >= mqtt_publish_interval_ms) {
            last_publish = now;
            mqtt_publish_state();
        }
    }
}

bool is_mqtt_connected() {
    return mqttClient.connected();
}

void mqtt_request_reconnect() {
    mqttClient.disconnect();
    data_lock(g_state);
    g_state.net.mqtt_ok = false;
    g_state.ui_needs_update = true;
    data_unlock(g_state);
}
