#pragma once

// ============================================================
// Board: ESP32-2424S012 (ESP32-C3-MINI-1U + GC9A01 240x240)
// ============================================================

// ---- Display (GC9A01 via SPI2) ----
#define PIN_LCD_MOSI    7
#define PIN_LCD_SCLK    6
#define PIN_LCD_CS      10
#define PIN_LCD_DC      2
#define PIN_LCD_RST     3
#define PIN_LCD_BL      11   // Backlight (PWM)

// ---- Touch (CST816S via I2C) — capacitive version only ----
#define PIN_TOUCH_SDA   4
#define PIN_TOUCH_SCL   5
#define PIN_TOUCH_INT   0
#define PIN_TOUCH_RST   1

// Touch coordinate transform (set to 1 if your panel is rotated/flipped)
#define TOUCH_SWAP_XY   0
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0

// ---- Display resolution ----
#define LCD_WIDTH       240
#define LCD_HEIGHT      240

// ---- LVGL draw buffer: 1/10 of screen height, double-buffered ----
#define LVGL_BUF_LINES  24   // 240 * 24 * 2 bytes = ~11 KB per buffer

// ---- FreeRTOS task config ----
#define TASK_DISPLAY_STACK  8192
#define TASK_DISPLAY_PRIO   5
#define TASK_DISPLAY_CORE   0  // ESP32-C3 is single-core (core 0 only)

#define TASK_NETWORK_STACK  16384
#define TASK_NETWORK_PRIO   4
#define TASK_NETWORK_CORE   0

// ---- WiFi ----
// Legacy static credentials (optional). WiFiManager uses saved credentials and
// opens a captive portal when setup is needed.
#define WIFI_SSID           ""
#define WIFI_PASSWORD       ""
#define WIFI_CONNECT_TIMEOUT_MS 10000

// WiFiManager captive portal settings
#define WIFI_MANAGER_AP_NAME          "FlightRadarSetup"
#define WIFI_MANAGER_AP_PASSWORD      ""      // empty = open portal
#define WIFI_MANAGER_PORTAL_TIMEOUT_S 180
#define WIFI_MANAGER_FORCE_PORTAL_ON_BOOT 0    // 1: always open portal to pick Wi-Fi
#define WIFI_MANAGER_CLEAR_SAVED_ON_BOOT 0     // 1: erase stored creds before portal

// ---- NTP ----
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.nist.gov"
#define NTP_GMT_OFFSET_SEC  0
#define NTP_DAYLIGHT_OFFSET_SEC 0

// ---- ADS-B Exchange ----
// No rate limits, community-run global ADS-B network
#define OPENSKY_FETCH_PERIOD_MS 5000
#define OPENSKY_HTTP_TIMEOUT_MS 15000

// ---- Boot map background ----
#define MAP_FETCH_ON_BOOT 1
#define MAP_HTTP_TIMEOUT_MS 20000
#define MAP_MAX_JPEG_BYTES 20480    // 20KB — enough for 120x120 JPEG

#ifndef GOOGLE_STATIC_MAPS_API_KEY
#define GOOGLE_STATIC_MAPS_API_KEY "AIzaSyAoitFE8LLEZqK-9hp6ErO6oFTDHhH_UHg"
#endif

// ---- Home location placeholder (set before flashing) ----
// These define the radar center. Replace with your actual coordinates.
#define HOME_LAT   51.5051316f   // Vaarsvelden, Best, Netherlands
#define HOME_LON    5.3728435f
#define RADAR_RADIUS_KM  20.0f

// ---- Serial baud ----
#define SERIAL_BAUD 115200
