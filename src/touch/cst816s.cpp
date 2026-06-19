#include "cst816s.h"
#include "../config.h"

#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t CST816S_ADDR_PRIMARY = 0x15;
constexpr uint8_t CST816S_ADDR_ALT = 0x14;
constexpr uint8_t REG_GESTURE = 0x01;
constexpr uint8_t REG_TOUCH_POINTS = 0x02;
constexpr uint8_t REG_XH = 0x03;
constexpr uint8_t REG_CHIP_ID = 0xA7;

bool s_ready = false;
bool s_touch_active = false;
uint32_t s_last_tap_ms = 0;
uint8_t s_addr = CST816S_ADDR_PRIMARY;

bool read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    size_t got = Wire.requestFrom((int)addr, (int)len, (int)true);
    if (got != len) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)Wire.read();
    }
    return true;
}

bool probe_addr(uint8_t addr) {
    uint8_t chip_id = 0;
    if (read_regs(addr, REG_CHIP_ID, &chip_id, 1)) {
        Serial.printf("[touch] probe addr=0x%02X chip_id=0x%02X\n", (unsigned)addr, (unsigned)chip_id);
        return true;
    }

    // Fallback: some clones may not respond to chip-id but do respond to touch-point register.
    uint8_t points = 0;
    if (read_regs(addr, REG_TOUCH_POINTS, &points, 1)) {
        Serial.printf("[touch] probe addr=0x%02X points-reg ok\n", (unsigned)addr);
        return true;
    }

    return false;
}

void apply_transform(int16_t *x, int16_t *y) {
    int16_t tx = *x;
    int16_t ty = *y;

#if TOUCH_SWAP_XY
    int16_t t = tx;
    tx = ty;
    ty = t;
#endif

#if TOUCH_INVERT_X
    tx = (int16_t)(LCD_WIDTH - 1 - tx);
#endif

#if TOUCH_INVERT_Y
    ty = (int16_t)(LCD_HEIGHT - 1 - ty);
#endif

    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx >= LCD_WIDTH) tx = LCD_WIDTH - 1;
    if (ty >= LCD_HEIGHT) ty = LCD_HEIGHT - 1;

    *x = tx;
    *y = ty;
}

} // namespace

bool cst816s_init() {
    pinMode(PIN_TOUCH_RST, OUTPUT);
    digitalWrite(PIN_TOUCH_RST, LOW);
    delay(5);
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(50);

    pinMode(PIN_TOUCH_INT, INPUT_PULLUP);

    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire.setClock(400000);

    if (probe_addr(CST816S_ADDR_PRIMARY)) {
        s_addr = CST816S_ADDR_PRIMARY;
        s_ready = true;
    } else if (probe_addr(CST816S_ADDR_ALT)) {
        s_addr = CST816S_ADDR_ALT;
        s_ready = true;
    } else {
        s_ready = false;
    }

    if (!s_ready) {
        Serial.println("[touch] CST816S not detected");
        return false;
    }

    Serial.printf("[touch] CST816S online at 0x%02X\n", (unsigned)s_addr);
    return true;
}

bool cst816s_poll_tap(int16_t *x, int16_t *y) {
    if (!s_ready || x == nullptr || y == nullptr) {
        return false;
    }

    uint8_t packet[6] = {0};
    if (!read_regs(s_addr, REG_GESTURE, packet, sizeof(packet))) {
        return false;
    }

    const uint8_t points = packet[1] & 0x0F;

    const bool touched_now = points > 0;

    // Touch-down edge triggers one event per tap.
    if (touched_now && !s_touch_active) {
        int16_t raw_x = (int16_t)(((packet[2] & 0x0F) << 8) | packet[3]);
        int16_t raw_y = (int16_t)(((packet[4] & 0x0F) << 8) | packet[5]);

        apply_transform(&raw_x, &raw_y);

        const uint32_t now = millis();
        if (now - s_last_tap_ms < 160) {
            s_touch_active = true;
            return false;
        }

        *x = raw_x;
        *y = raw_y;
        Serial.printf("[touch] tap x=%d y=%d points=%u\n", (int)*x, (int)*y, (unsigned)points);
        s_last_tap_ms = now;
        s_touch_active = true;
        return true;
    }

    if (!touched_now) {
        s_touch_active = false;
    }

    return false;
}
