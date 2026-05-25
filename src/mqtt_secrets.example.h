#ifndef MQTT_SECRETS_EXAMPLE_H
#define MQTT_SECRETS_EXAMPLE_H

// Copy this file to src/mqtt_secrets.h for local development.
// Never commit real broker credentials.

#define MQTT_SERVER_DEFAULT       "your-broker.example.com"
#define MQTT_PORT_SECURE_DEFAULT  8883
#define MQTT_PORT_NORMAL_DEFAULT  1883
#define MQTT_USER_DEFAULT         "your-username"
#define MQTT_PASS_DEFAULT         "your-password"
#define MQTT_TOPIC_SUB_DEFAULT    "your/command/topic"
#define MQTT_TOPIC_PUB_DEFAULT    "your/state/topic"
#define MQTT_DEVICE_NAME_DEFAULT  "Meeting Room Master"
#define MQTT_FW_VERSION_DEFAULT   "1.0.0"
#define MQTT_PUBLISH_INTERVAL_MS  5000

#endif
