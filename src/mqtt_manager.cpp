#include "mqtt_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "data.h"

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
#define MQTT_FW_VERSION_DEFAULT   "1.0.0"
#define MQTT_PUBLISH_INTERVAL_MS  5000
#endif

const char* mqtt_server       = MQTT_SERVER_DEFAULT;
const int   mqtt_port_secure  = MQTT_PORT_SECURE_DEFAULT;
const int   mqtt_port_normal  = MQTT_PORT_NORMAL_DEFAULT;
const char* mqtt_user         = MQTT_USER_DEFAULT;
const char* mqtt_pass         = MQTT_PASS_DEFAULT;
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

static void mqtt_publish_state() {
    JsonDocument doc;
    doc["type"] = "smart_building_master_state";
    doc["device_name"] = mqtt_device_name;
    doc["firmware_version"] = mqtt_fw_version;
    doc["publisher"] = "HK";
    doc["lab"] = "Computer Engineering Lab";
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

    bool ok = mqttClient.publish(mqtt_topic_pub, mqtt_publish_payload, payload_len);
    Serial.printf("[MQTT] Publish %s topic=%s bytes=%u\n",
                  ok ? "OK" : "FAIL",
                  mqtt_topic_pub,
                  (unsigned)payload_len);
}

static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.printf("[MQTT] JSON parse failed: %s\n", error.c_str());
        return;
    }

    if (strcmp(topic, mqtt_topic_sub) == 0) {
        const char* type = doc["type"] | "";
        if (strcmp(type, "smart_building_master_state") == 0) {
            return;
        }

        if (strcmp(type, "smart_building_master_command") == 0) {
            JsonObject controls = doc["controls"];
            data_lock(g_state);
            if (!controls["projector"]["power"].isNull()) {
                g_state.sensor.projector_on = controls["projector"]["power"].as<bool>();
            }
            if (!controls["ac"]["power"].isNull()) {
                g_state.sensor.ac_on = controls["ac"]["power"].as<bool>();
            }
            if (!controls["ac"]["target_c"].isNull()) {
                g_state.sensor.temp_target = controls["ac"]["target_c"].as<float>();
            }
            if (controls["lights"].is<JsonArray>()) {
                for (JsonObject light : controls["lights"].as<JsonArray>()) {
                    if (!light["power"].isNull()) {
                        g_state.sensor.light_on = light["power"].as<bool>();
                    }
                }
            }
            g_state.ui_needs_update = true;
            data_unlock(g_state);
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

static void reconnect() {
    if (!mqttClient.connected()) {
        if (g_state.net.net_priority == 1 && g_state.net.lan_connected) {
            mqttClient.setClient(ethClient);
            mqttClient.setServer(mqtt_server, mqtt_port_normal);
            Serial.print("[MQTT] Connecting via LAN...");
        } else if (g_state.net.wifi_connected) {
            mqttClient.setClient(secureClient);
            mqttClient.setServer(mqtt_server, mqtt_port_secure);
            Serial.print("[MQTT] Connecting via WiFi (SSL)...");
        } else {
            return;
        }

        String clientId = "MasterS3-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("OK");
            mqttClient.subscribe(mqtt_topic_sub);
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
