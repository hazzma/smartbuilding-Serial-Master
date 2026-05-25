#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include <Wire.h>
#include "display.h"  // TOUCH_SDA, TOUCH_SCL, TOUCH_RST pin defs

// Common capacitive touch I2C addresses.
#define FT6236U_ADDR    0x38
#define GT911_ADDR_1    0x5D
#define GT911_ADDR_2    0x14

// FT6236U Register Map
#define FT_REG_NUM_FINGER   0x02
#define FT_REG_P1_XH        0x03
#define FT_REG_P1_XL        0x04
#define FT_REG_P1_YH        0x05
#define FT_REG_P1_YL        0x06
#define FT_REG_THRESHHOLD   0x80
#define FT_REG_CTRL         0x86  // 0=normal, 1=monitor, 3=standby
#define FT_REG_PERIOD_ACTIVE 0x88

enum TouchEventType {
    TOUCH_EVENT_NONE = 0,
    TOUCH_EVENT_DOWN,
    TOUCH_EVENT_MOVE,
    TOUCH_EVENT_UP
};

void touch_init();
bool touch_get_point(int &tx, int &ty);
bool touch_get_event(int &tx, int &ty, TouchEventType &event);

#endif
