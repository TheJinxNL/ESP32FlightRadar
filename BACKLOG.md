# ESP32 Round Display — Airplane Radar Backlog

## Architecture

```
FreeRTOS Scheduler
├── Display Task   — LVGL + Radar UI (LovyanGFX → GC9A01 SPI)
├── Network Task   — WiFi + OpenSky REST API
└── Touch Task     — CST816S I2C tap events

Shared State
└── FlightData[]   — mutex-protected ring buffer
```

**Data source**: OpenSky Network REST API (free, anonymous, 1 req/10s)
`GET https://opensky-network.org/api/states/all?lamin=…&lomin=…&lamax=…&lomax=…`

---

## Milestone 1 — Project Scaffold + LVGL "Hello Radar"

**Goal**: PlatformIO builds and flashes. Board boots, renders styled text via LVGL on a dedicated FreeRTOS display task.

### Tasks
- [ ] Create `platformio.ini` with ESP32-C3 board, Arduino framework, LovyanGFX and LVGL libs
- [ ] Create `config.h` with all pin definitions and placeholder home coordinates
- [ ] Implement `display/display.cpp` — LovyanGFX init, LVGL `lv_init()`, flush callback, tick source
- [ ] Implement `display/ui.cpp` — LVGL screen with label "RADAR INIT" in green on black
- [ ] Launch display task in `main.cpp` via `xTaskCreatePinnedToCore`
- [ ] Configure LVGL `lv_conf.h` for 240×240, 16-bit color, two draw buffers

### Acceptance Criteria
- [ ] Firmware compiles without warnings
- [ ] Board boots and shows "RADAR INIT" label centered on the circular screen
- [ ] No crash or watchdog reset after 60 seconds
- [ ] FreeRTOS heap usage reported on serial is stable

---

## Milestone 2 — WiFi + NTP + OpenSky API Fetch

**Goal**: Board connects to WiFi, syncs time via NTP, fetches and parses live flight JSON from OpenSky, logs results to serial.

### Tasks
- [ ] Implement `model/flight.h` — define `FlightData` struct (ICAO, callsign, lat, lon, altitude, speed, heading, country)
- [ ] Implement `network/wifi_manager.cpp` — connect with retry, log IP on serial
- [ ] Implement `network/ntp.cpp` — sync system time, expose `bool isTimeSynced()`
- [ ] Implement `network/flight_data.cpp` — build bounding-box URL from home coords + radius, HTTP GET, ArduinoJson parse into `FlightData[]`
- [ ] Create network task in `main.cpp`, protect `FlightData[]` with a FreeRTOS mutex
- [ ] Update UI label to show WiFi status ("CONNECTING…" → "ONLINE")

### Acceptance Criteria
- [ ] Board connects to WiFi within 10 seconds of boot
- [ ] Serial prints NTP-synced UTC time
- [ ] Serial prints at least one parsed aircraft entry (ICAO + callsign + lat/lon) within 30 seconds
- [ ] Network task repeats fetch every 10 seconds without memory leak (heap stable over 5 minutes)
- [ ] Board recovers and reconnects if WiFi is briefly disabled

---

## Milestone 3 — Radar Canvas + Sweep Animation

**Goal**: Radar background and rotating sweep line rendered on LVGL canvas. No real data — mock flights used. Looks like a radar.

### Tasks
- [ ] Implement `display/radar_math.cpp` — convert lat/lon offset → pixel (x, y) relative to 120,120 center
- [ ] Implement `display/radar_view.cpp` — draw 3 range rings (10 km / 25 km / 50 km), cardinal labels N/E/S/W, center dot
- [ ] Add rotating sweep line using LVGL canvas pixel writes; one full rotation every 4 seconds
- [ ] Add fading green trail behind sweep (alpha decay per frame)
- [ ] Wire LVGL animation timer to sweep angle update; target 30 fps
- [ ] Add range label overlay (e.g. "50 km" at outer ring)

### Acceptance Criteria
- [ ] Radar rings and cardinal labels are visible and correctly spaced on the 240×240 circle
- [ ] Sweep line rotates smoothly without visible tearing or stutter
- [ ] LVGL task CPU usage stays below 60% (measured via `uxTaskGetSystemState`)
- [ ] Fading trail is visible and fades within one full rotation
- [ ] Display task heap usage is stable after 5 minutes

---

## Milestone 4 — Plot Aircraft on Radar

**Goal**: Real aircraft from OpenSky appear as dots on the radar. Positions update every 10 seconds.

### Tasks
- [ ] Implement `display/aircraft_layer.cpp` — iterate `FlightData[]` under mutex, project each to pixel, draw dot
- [ ] Draw callsign label next to dot (small font, truncated to 6 chars if needed)
- [ ] Flash dot brighter for 2 seconds when aircraft first appears (newly detected)
- [ ] Filter out aircraft outside configured radius before plotting
- [ ] Handle empty response gracefully (no crash, dots simply absent)

### Acceptance Criteria
- [ ] At least one aircraft dot appears on the radar during a period of air traffic (verify against FlightRadar24 for approximate position match)
- [ ] Dots update position after each 10-second fetch cycle
- [ ] Callsign is readable next to dot (no overlap with rings)
- [ ] No crash when zero aircraft are in range
- [ ] No crash when OpenSky returns a malformed or empty response

---

## Milestone 5 — Touch: Aircraft Detail Panel

**Goal**: Tapping a dot opens a detail panel. Tapping outside closes it.

### Tasks
- [ ] Implement `input/touch.cpp` — CST816S I2C init, interrupt-driven tap read, push `TouchEvent{x, y}` to FreeRTOS queue
- [ ] Create touch task in `main.cpp`; forward events to UI layer
- [ ] Implement `display/detail_panel.cpp` — LVGL panel with callsign, altitude (m), speed (km/h), heading (°), origin country
- [ ] On tap: find nearest aircraft within 15px radius; if found, open panel
- [ ] Auto-close panel after 5 seconds or on tap outside
- [ ] Show "No aircraft selected" state if tap lands on empty space

### Acceptance Criteria
- [ ] Tapping within 15px of a dot opens the detail panel
- [ ] All five data fields are readable on the 240×240 screen
- [ ] Tapping outside the panel closes it
- [ ] Panel auto-closes after 5 seconds
- [ ] Touch task does not interfere with display task frame rate

---

## Milestone 6 — Configuration + Stability

**Goal**: WiFi credentials and home location are stored in NVM. Firmware runs 24 hours without crash or memory leak.

### Tasks
- [ ] Implement `config/preferences.cpp` — read/write WiFi SSID, password, home lat/lon, radius from ESP32 `Preferences` (NVM)
- [ ] On first boot (no NVM entry), fall back to compile-time defaults in `config.h`
- [ ] Add watchdog timer (60s window)
- [ ] Add WiFi reconnect loop in network task (exponential backoff, max 60s)
- [ ] Dim backlight to 20% after 30 seconds of no touch input; restore on tap
- [ ] Log heap, stack high-water marks to serial every 60 seconds

### Acceptance Criteria
- [ ] NVM credentials persist across power cycles (verified by removing `config.h` defaults)
- [ ] Watchdog triggers a clean reboot if display task stalls (tested by inserting a deliberate delay)
- [ ] WiFi reconnects automatically after router reboot without requiring firmware reboot
- [ ] 24-hour soak test: no crash, heap usage within ±5 KB of initial value, at least one aircraft plotted per hour
- [ ] Backlight dims after 30s idle and restores on touch

---

## Library Reference

| Library | Version | Purpose |
|---|---|---|
| LovyanGFX | ^1.1.16 | GC9A01 SPI driver + LVGL flush backend |
| LVGL | ^8.4.0 | UI, canvas, animation, touch input |
| ArduinoJson | ^7.0.0 | OpenSky JSON parsing |
| CST816S | latest | Capacitive touch driver |
| HTTPClient | built-in | OpenSky REST requests |
| Preferences | built-in | NVM config storage |

## Pin Reference (ESP32-2424S012)

| Signal | GPIO |
|---|---|
| LCD MOSI | 7 |
| LCD SCLK | 6 |
| LCD CS | 10 |
| LCD DC | 2 |
| LCD RST | 3 |
| LCD Backlight | 11 |
| Touch SDA | 4 |
| Touch SCL | 5 |
| Touch INT | 0 |
| Touch RST | 1 |
