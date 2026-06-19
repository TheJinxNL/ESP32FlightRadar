#include "ntp.h"
#include "../config.h"

#include <Arduino.h>
#include <time.h>

static bool s_ntp_started = false;

void ntp_begin() {
    if (s_ntp_started) {
        return;
    }
    configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
    s_ntp_started = true;
}

bool ntp_is_synced() {
    time_t now = 0;
    time(&now);
    return now > 1700000000;  // sanity threshold (~2023)
}

bool ntp_sync_blocking(unsigned long timeout_ms) {
    ntp_begin();

    const uint32_t start = millis();
    while (!ntp_is_synced() && (millis() - start) < timeout_ms) {
        delay(200);
    }

    if (!ntp_is_synced()) {
        Serial.println("[ntp] sync timeout");
        return false;
    }

    struct tm info;
    if (getLocalTime(&info, 1000)) {
        char buf[48];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &info);
        Serial.printf("[ntp] synced local time: %s\n", buf);
    } else {
        Serial.println("[ntp] synced");
    }
    return true;
}
