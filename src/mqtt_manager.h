#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>

void mqtt_init();
void mqtt_loop();
bool is_mqtt_connected();

#endif
