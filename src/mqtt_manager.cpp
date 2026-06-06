#include "mqtt_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "data.h"
#include "rs485_manager.h"

#if __has_include("mqtt_secrets.h")
#include "mqtt_secrets.h"
#endif

#ifndef MQTT_SERVER_DEFAULT
#define MQTT_SERVER_DEFAULT       "your-broker.example.com"
#define MQTT_PORT_SECURE_DEFAULT  8883
#define MQTT_PORT_NORMAL_DEFAULT  1883
#define MQTT_USER_DEFAULT         ""
#define MQTT_PASS_DEFAULT         ""
#define MQTT_TOPIC_SUB_DEFAULT    "smartbuilding/master/command"
#define MQTT_TOPIC_PUB_DEFAULT    "smartbuilding/master/state"
#define MQTT_DEVICE_NAME_DEFAULT  "Meeting Room Master"
#define MQTT_FW_VERSION_DEFAULT   "V2"
#define MQTT_PUBLISH_INTERVAL_MS  5000
#endif

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

enum MqttTopicKind : uint8_t {
    MQTT_TOPIC_SUHU,
    MQTT_TOPIC_CO2,
    MQTT_TOPIC_LUX,
    MQTT_TOPIC_HUMAN,
    MQTT_TOPIC_LED,
    MQTT_TOPIC_AC,
    MQTT_TOPIC_PROJECTOR,
    MQTT_TOPIC_MASTER
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
        case MQTT_TOPIC_SUHU: suffix = "suhu"; break;
        case MQTT_TOPIC_CO2: suffix = "co2"; break;
        case MQTT_TOPIC_LUX: suffix = "lux"; break;
        case MQTT_TOPIC_HUMAN: suffix = "human"; break;
        case MQTT_TOPIC_LED: suffix = "led"; break;
        case MQTT_TOPIC_AC: suffix = "ac"; break;
        case MQTT_TOPIC_PROJECTOR: suffix = "projector"; break;
        case MQTT_TOPIC_MASTER: suffix = "master"; break;
    }
    snprintf(out, out_len, "Class %s %s", mqtt_class_name_locked(), suffix);
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
    char topic_suhu[48];
    char topic_co2[48];
    char topic_lux[48];
    char topic_human[48];
    char topic_led[48];
    char topic_ac[48];
    char topic_projector[48];
    char topic_master[48];

    JsonDocument temp_doc;
    JsonDocument led_doc;
    JsonDocument ac_doc;
    JsonDocument projector_doc;
    JsonDocument master_doc;

    char co2_payload[16];
    char lux_payload[16];
    char human_payload[8];

    data_lock(g_state);

    mqtt_build_topic_locked(MQTT_TOPIC_SUHU, topic_suhu, sizeof(topic_suhu));
    mqtt_build_topic_locked(MQTT_TOPIC_CO2, topic_co2, sizeof(topic_co2));
    mqtt_build_topic_locked(MQTT_TOPIC_LUX, topic_lux, sizeof(topic_lux));
    mqtt_build_topic_locked(MQTT_TOPIC_HUMAN, topic_human, sizeof(topic_human));
    mqtt_build_topic_locked(MQTT_TOPIC_LED, topic_led, sizeof(topic_led));
    mqtt_build_topic_locked(MQTT_TOPIC_AC, topic_ac, sizeof(topic_ac));
    mqtt_build_topic_locked(MQTT_TOPIC_PROJECTOR, topic_projector, sizeof(topic_projector));
    mqtt_build_topic_locked(MQTT_TOPIC_MASTER, topic_master, sizeof(topic_master));

    temp_doc["sensor"] = "temperature";
    temp_doc["unit"] = "celsius";
    JsonArray values = temp_doc["values"].to<JsonArray>();
    float sum = 0.0f;
    uint8_t valid_temp = 0;
    for (uint8_t i = 0; i < DASHBOARD_TEMP_SLOTS; i++) {
        if (g_state.rs485.dashboard.temp_valid[i]) {
            float value = g_state.rs485.dashboard.temp[i];
            values.add(value);
            sum += value;
            valid_temp++;
        } else {
            values.add(nullptr);
        }
    }
    if (valid_temp > 0) temp_doc["avg_c"] = sum / valid_temp;
    else temp_doc["avg_c"].set(nullptr);
    temp_doc["timestamp_ms"] = millis();

    if (g_state.rs485.dashboard.co2_valid) snprintf(co2_payload, sizeof(co2_payload), "%d", g_state.rs485.dashboard.co2);
    else snprintf(co2_payload, sizeof(co2_payload), "-1");

    if (g_state.rs485.dashboard.lux_valid) snprintf(lux_payload, sizeof(lux_payload), "%d", (int)g_state.rs485.dashboard.lux);
    else snprintf(lux_payload, sizeof(lux_payload), "-1");

    if (g_state.rs485.dashboard.human_presence_valid) snprintf(human_payload, sizeof(human_payload), "%u", g_state.rs485.dashboard.human_presence ? 1 : 0);
    else snprintf(human_payload, sizeof(human_payload), "-1");

    led_doc["actuator"] = "led";
    JsonArray positions = led_doc["positions"].to<JsonArray>();
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
            JsonObject pos = positions.add<JsonObject>();
            pos["id"] = light_id;
            pos["state"] = slave.relay_state[relay] ? "ON" : "OFF";
            light_id++;
        }
    }
    led_doc["timestamp_ms"] = millis();

    ac_doc["actuator"] = "ac";
    ac_doc["available"] = g_state.rs485.dashboard.ac_available;
    ac_doc["status_type"] = "command_result";
    ac_doc["physical_state_verified"] = false;
    ac_doc["mirror_ac_1_ac_2"] = mqtt_has_ir_combo_locked();
    ac_doc["power"] = g_state.sensor.ac_on ? "ON" : "OFF";
    ac_doc["target_c"] = g_state.sensor.temp_target;
    ac_doc["timestamp_ms"] = millis();

    projector_doc["actuator"] = "projector";
    projector_doc["available"] = g_state.rs485.dashboard.projector_available;
    projector_doc["status_type"] = "command_result";
    projector_doc["physical_state_verified"] = false;
    projector_doc["power"] = g_state.sensor.projector_on ? "ON" : "OFF";
    projector_doc["timestamp_ms"] = millis();

    master_doc["type"] = "smart_building_master_status";
    master_doc["device_name"] = mqtt_device_name_locked();
    master_doc["class_name"] = mqtt_class_name_locked();
    master_doc["firmware_version"] = mqtt_fw_version;
    master_doc["firmware_by"] = "Hansel Kay CE LAB";
    master_doc["wifi_connected"] = g_state.net.wifi_connected;
    master_doc["lan_connected"] = g_state.net.lan_connected;
    master_doc["mqtt_connected"] = mqttClient.connected();
    master_doc["rs485_bus_ok"] = g_state.rs485.bus_ok;
    master_doc["device_registry_status"] = mqtt_pairing_candidate_unknown_locked()
                                               ? "UNPAIRED_DEVICE_DETECTED"
                                               : "OK";
    master_doc["timestamp_ms"] = millis();

    JsonArray registry = master_doc["registry"].to<JsonArray>();
    uint8_t registry_count = g_state.rs485.slave_count;
    if (registry_count > RS485_MAX_SLAVES) registry_count = RS485_MAX_SLAVES;
    for (uint8_t i = 0; i < registry_count; i++) {
        const RS485SlaveState& slave = g_state.rs485.slaves[i];
        if (slave.address == 0 && slave.uid == 0 && slave.mac == 0 && !slave.online) continue;

        JsonObject item = registry.add<JsonObject>();
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
    }

    if (mqtt_pairing_candidate_unknown_locked()) {
        JsonObject unknown = master_doc["pairing_candidate"].to<JsonObject>();
        const RS485SlaveState& candidate = g_state.rs485.pairing_candidate;
        unknown["status"] = "UNPAIRED_DEVICE_DETECTED";
        unknown["address"] = candidate.address;
        char uid_buf[12];
        snprintf(uid_buf, sizeof(uid_buf), "%08lX", (unsigned long)candidate.uid);
        unknown["uid"] = uid_buf;
        char mac_buf[18];
        mqtt_format_mac(candidate.mac, mac_buf, sizeof(mac_buf));
        unknown["mac"] = mac_buf;
        unknown["profile"] = device_profile_name(candidate.profile);
    }

    data_unlock(g_state);

    mqtt_publish_json(topic_suhu, temp_doc, true, "temperature");
    mqtt_publish_raw(topic_co2, co2_payload, true, "co2");
    mqtt_publish_raw(topic_lux, lux_payload, true, "lux");
    mqtt_publish_raw(topic_human, human_payload, true, "human");
    mqtt_publish_json(topic_led, led_doc, true, "led");
    mqtt_publish_json(topic_ac, ac_doc, true, "ac");
    mqtt_publish_json(topic_projector, projector_doc, true, "projector");
    mqtt_publish_json(topic_master, master_doc, true, "master-status");
}

static void mqtt_publish_state() {
    mqtt_publish_v2_state();
    mqtt_publish_legacy_state();
}

static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    char topic_led[48];
    char topic_ac[48];
    char topic_projector[48];
    data_lock(g_state);
    mqtt_build_topic_locked(MQTT_TOPIC_LED, topic_led, sizeof(topic_led));
    mqtt_build_topic_locked(MQTT_TOPIC_AC, topic_ac, sizeof(topic_ac));
    mqtt_build_topic_locked(MQTT_TOPIC_PROJECTOR, topic_projector, sizeof(topic_projector));
    data_unlock(g_state);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.printf("[MQTT] JSON parse failed: %s\n", error.c_str());
        return;
    }

    if (strcmp(topic, topic_led) == 0) {
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
        if (!doc["available"].isNull() && doc["power"].is<const char*>()) return;

        bool desired_power = false;
        float desired_target = 0.0f;
        data_lock(g_state);
        if (!doc["power"].isNull()) {
            if (doc["power"].is<const char*>()) {
                const char* power = doc["power"] | "";
                g_state.sensor.ac_on = strcmp(power, "ON") == 0 || strcmp(power, "on") == 0;
            } else {
                g_state.sensor.ac_on = doc["power"].as<bool>();
            }
        }
        if (!doc["target_c"].isNull()) {
            g_state.sensor.temp_target = doc["target_c"].as<float>();
        }
        desired_power = g_state.sensor.ac_on;
        desired_target = g_state.sensor.temp_target;
        g_state.ui_needs_update = true;
        data_unlock(g_state);
        rs485_request_ac_command(desired_power, desired_target);
        Serial.println("[MQTT] AC command applied");
        mqtt_publish_state();
        return;
    }

    if (strcmp(topic, topic_projector) == 0) {
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

    if (strcmp(topic, mqtt_topic_sub) == 0) {
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
            bool light_changed = false;
            bool light_power = false;
            data_lock(g_state);
            if (!controls["projector"]["power"].isNull()) {
                g_state.sensor.projector_on = controls["projector"]["power"].as<bool>();
                projector_changed = true;
                projector_power = g_state.sensor.projector_on;
            }
            if (!controls["ac"]["power"].isNull()) {
                g_state.sensor.ac_on = controls["ac"]["power"].as<bool>();
                ac_changed = true;
            }
            if (!controls["ac"]["target_c"].isNull()) {
                g_state.sensor.temp_target = controls["ac"]["target_c"].as<float>();
                ac_changed = true;
            }
            ac_power = g_state.sensor.ac_on;
            ac_target = g_state.sensor.temp_target;
            if (controls["lights"].is<JsonArray>()) {
                for (JsonObject light : controls["lights"].as<JsonArray>()) {
                    if (!light["power"].isNull()) {
                        g_state.sensor.light_on = light["power"].as<bool>();
                        light_changed = true;
                        light_power = g_state.sensor.light_on;
                    }
                }
            }
            g_state.ui_needs_update = true;
            data_unlock(g_state);
            if (projector_changed) rs485_request_projector_command(projector_power);
            if (ac_changed) rs485_request_ac_command(ac_power, ac_target);
            if (light_changed) rs485_request_light_command(light_power);
            Serial.println("[MQTT] Command applied");
            mqtt_publish_state();
            return;
        }

        if (doc["temperature"].isNull() && doc["lux"].isNull() && doc["co2"].isNull()) {
            return;
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
    char topic[48];

    data_lock(g_state);
    mqtt_build_topic_locked(MQTT_TOPIC_LED, topic, sizeof(topic));
    data_unlock(g_state);
    mqttClient.subscribe(topic, 1);
    Serial.printf("[MQTT] Subscribe LED cmd topic=%s qos=1 retain=false\n", topic);

    data_lock(g_state);
    mqtt_build_topic_locked(MQTT_TOPIC_AC, topic, sizeof(topic));
    data_unlock(g_state);
    mqttClient.subscribe(topic, 1);
    Serial.printf("[MQTT] Subscribe AC cmd topic=%s qos=1 retain=false\n", topic);

    data_lock(g_state);
    mqtt_build_topic_locked(MQTT_TOPIC_PROJECTOR, topic, sizeof(topic));
    data_unlock(g_state);
    mqttClient.subscribe(topic, 1);
    Serial.printf("[MQTT] Subscribe Projector cmd topic=%s qos=1 retain=false\n", topic);
}

static void reconnect() {
    if (!mqttClient.connected()) {
        char server[64];
        char user[32];
        char pass[64];
        uint16_t port = 0;
        bool use_tls = false;
        data_lock(g_state);
        strncpy(server, g_state.net.mqtt_server, sizeof(server) - 1);
        server[sizeof(server) - 1] = '\0';
        strncpy(user, g_state.net.mqtt_user, sizeof(user) - 1);
        user[sizeof(user) - 1] = '\0';
        strncpy(pass, g_state.net.mqtt_pass, sizeof(pass) - 1);
        pass[sizeof(pass) - 1] = '\0';
        port = g_state.net.mqtt_port;
        use_tls = g_state.net.mqtt_use_tls;
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
            return;
        }

        String clientId = "MasterS3-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str(), user, pass)) {
            Serial.println("OK");
            mqttClient.subscribe(mqtt_topic_sub, 1);
            mqtt_subscribe_v2_topics();
            data_lock(g_state);
            g_state.net.mqtt_ok = true;
            data_unlock(g_state);
            mqtt_publish_state();
        } else {
            Serial.printf("Failed (rc=%d)\n", mqttClient.state());
            data_lock(g_state);
            g_state.net.mqtt_ok = false;
            data_unlock(g_state);
        }
    }
}

void mqtt_loop() {
    bool has_net = g_state.net.wifi_connected || g_state.net.lan_connected;
    if (!has_net) return;

    if (!mqttClient.connected()) {
        static uint32_t last_reconnect = 0;
        if (millis() - last_reconnect > 5000) {
            last_reconnect = millis();
            reconnect();
        }
    } else {
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
