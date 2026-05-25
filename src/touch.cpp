#include "touch.h"

static bool touch_online = false;
static uint8_t touch_addr = FT6236U_ADDR;

// Touch sensor is portrait (0-320 x 0-480), display is landscape (480x320).
// Display rotation is 3, which is 180 degrees from the previous rotation 1 mounting.
#define TOUCH_SWAP_XY    true
#define TOUCH_FLIP_X     true
#define TOUCH_FLIP_Y     false
#define TOUCH_MAX_X      480
#define TOUCH_MAX_Y      320

static bool i2c_ping(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

static void touch_scan_bus() {
    Serial.printf("[TC] I2C scan SDA:%d SCL:%d\n", TOUCH_SDA, TOUCH_SCL);
    bool found = false;

    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_ping(addr)) {
            Serial.printf("[TC] I2C device found at 0x%02X\n", addr);
            found = true;
        }
    }

    if (!found) Serial.println("[TC] I2C scan: no devices found");
}

static bool touch_detect() {
    const uint8_t candidates[] = { FT6236U_ADDR, GT911_ADDR_1, GT911_ADDR_2 };

    for (uint8_t i = 0; i < sizeof(candidates); i++) {
        if (i2c_ping(candidates[i])) {
            touch_addr = candidates[i];
            return true;
        }
    }

    return false;
}

static void ft_read_data(uint8_t* buf, uint8_t len) {
    if (!touch_online) return;

    Wire.beginTransmission(touch_addr);
    Wire.write(FT_REG_NUM_FINGER);
    if (Wire.endTransmission(false) != 0) {
        touch_online = false;
        return;
    }

    if (Wire.requestFrom(touch_addr, len) == len) {
        for (uint8_t i = 0; i < len; i++) {
            buf[i] = Wire.read();
        }
    } else {
        touch_online = false;
    }
}

void touch_init() {
    Serial.println("[TC] --- Touch Initialization ---");

    pinMode(TOUCH_SDA, INPUT_PULLUP);
    pinMode(TOUCH_SCL, INPUT_PULLUP);
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, HIGH); delay(10);
    digitalWrite(TOUCH_RST, LOW);  delay(50);
    digitalWrite(TOUCH_RST, HIGH); delay(300);

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000);

    touch_scan_bus();

    touch_online = touch_detect();

    if (touch_online) {
        Serial.printf("[TC] Touch controller detected at 0x%02X\n", touch_addr);
        if (touch_addr != FT6236U_ADDR) {
            Serial.println("[TC] Warning: parser is FT6236U-style; GT911 needs a different parser.");
        }
    } else {
        Serial.println("[TC] Touch controller NOT FOUND!");
    }
}

static uint32_t last_scan = 0;
static bool     touch_was_pressed = false;
static uint32_t last_valid_press  = 0;
static int      last_touch_x = 0;
static int      last_touch_y = 0;
static uint32_t last_move_ts = 0;

static bool touch_read_current(int &tx, int &ty, bool &pressed) {
    uint32_t now = millis();
    pressed = false;

    if (!touch_online && (now - last_scan > 2000)) {
        last_scan = now;
        touch_online = touch_detect();
        Serial.printf("[TC] Retry detect: %s", touch_online ? "FOUND" : "not found");
        if (touch_online) Serial.printf(" at 0x%02X", touch_addr);
        Serial.println();
    }

    if (!touch_online) return false;

    uint8_t data[7] = {0};
    ft_read_data(data, 5);

    if (!touch_online) return false;

    uint8_t num_fingers = data[0] & 0x0F;
    if (num_fingers == 0 || num_fingers > 2) {
        return true;
    }

    int raw_x = ((data[1] & 0x0F) << 8) | data[2];
    int raw_y = ((data[3] & 0x0F) << 8) | data[4];
    uint8_t event = (data[1] >> 6) & 0x03;

    if (event == 1) {
        return true;
    }

    int final_x = raw_x;
    int final_y = raw_y;

    if (TOUCH_SWAP_XY) {
        int temp = final_x;
        final_x = final_y;
        final_y = temp;
    }

    if (TOUCH_FLIP_X) final_x = (TOUCH_MAX_X - 1) - final_x;
    if (TOUCH_FLIP_Y) final_y = (TOUCH_MAX_Y - 1) - final_y;

    tx = final_x;
    ty = final_y;
    pressed = true;
    return true;
}

bool touch_get_event(int &tx, int &ty, TouchEventType &event) {
    event = TOUCH_EVENT_NONE;

    bool pressed = false;
    if (!touch_read_current(tx, ty, pressed)) return false;

    uint32_t now = millis();

    if (!pressed) {
        if (touch_was_pressed) {
            touch_was_pressed = false;
            tx = last_touch_x;
            ty = last_touch_y;
            event = TOUCH_EVENT_UP;
            return true;
        }
        return false;
    }

    if (!touch_was_pressed && (now - last_valid_press > 80)) {
        touch_was_pressed = true;
        last_valid_press = now;
        last_move_ts = now;
        last_touch_x = tx;
        last_touch_y = ty;
        event = TOUCH_EVENT_DOWN;
        Serial.printf("[TC] Touch down X:%d Y:%d\n", tx, ty);
        return true;
    }

    if (!touch_was_pressed) return false;

    int dx = abs(tx - last_touch_x);
    int dy = abs(ty - last_touch_y);
    if ((dx >= 2 || dy >= 2) && (now - last_move_ts >= 20)) {
        last_touch_x = tx;
        last_touch_y = ty;
        last_move_ts = now;
        event = TOUCH_EVENT_MOVE;
        return true;
    }

    return false;
}

bool touch_get_point(int &tx, int &ty) {
    TouchEventType event = TOUCH_EVENT_NONE;
    if (!touch_get_event(tx, ty, event)) return false;

    return event == TOUCH_EVENT_DOWN;
}
