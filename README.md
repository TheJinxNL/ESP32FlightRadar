# ESP32 Round Display Flight Radar

A real-time flight radar for the ESP32-C3 round display (GC9A01 + CST816S), built with LVGL and PlatformIO.

This firmware renders nearby aircraft on a circular radar UI with a Google Static Maps background, supports touch selection, and fetches route details (departure/arrival/IATA flight number) on demand.

## Quick Start

1. Install VS Code and the PlatformIO extension.
2. Clone this repo and open the `firmware` folder in VS Code.
3. Edit `src/config.h`:
  - Set `WIFI_SSID` and `WIFI_PASSWORD` (or leave blank to use the WiFiManager captive portal)
  - Set `HOME_LAT`, `HOME_LON`, and `RADAR_RADIUS_KM`
  - Set `GOOGLE_STATIC_MAPS_API_KEY` for the boot-time map background
4. Build:

```bash
/home/$USER/.platformio/penv/bin/pio run --environment esp32-c3-devkitm-1
```

5. Upload:

```bash
/home/$USER/.platformio/penv/bin/pio run --target upload --environment esp32-c3-devkitm-1
```

6. Open serial monitor:

```bash
/home/$USER/.platformio/penv/bin/pio device monitor --environment esp32-c3-devkitm-1
```

7. Tap an aircraft on the radar to fetch route details.

## Features

- Live aircraft positions from community ADS-B data (adsb.lol, plain HTTP — no TLS overhead)
- Google Static Maps background image fetched once at boot (120×120 JPEG, zoomed 2× by LVGL, pre-dimmed to 20% brightness)
- Smooth radar sweep animation on 240×240 round display (LVGL 8.x)
- WiFiManager captive portal for Wi-Fi setup without reflashing
- Touch selection of aircraft
- On-demand route lookup (dep/arr + IATA flight number) when aircraft is tapped
- Flight details panel shows:
  - ICAO callsign (left) and IATA flight number e.g. `FR5682` (right-aligned)
  - `DEP EIN  ARR STN` route line
  - Altitude (m) and ground speed (m/s)
  - `Retrieving...` while waiting, `N/A` on timeout
- Aircraft icons by type: Plane / Helicopter / Balloon
- IATA-first airport codes (3-letter), ICAO fallback
- API failure backoff: 30 s pause after 3 consecutive failures to protect heap
- FreeRTOS dual-task architecture (display + network)

## Hardware

This project targets the ESP32-C3 1.28 inch round-display boards described in `hardware.pdf`.

Hardware family (from the datasheet):

- `ESP32-2424S012N-I` (non-touch)
- `ESP32-2424S012C-I` (capacitive touch)
- `ESP32-2424S012C-I-Y(W)` (capacitive touch, white case)
- `ESP32-2424S012C-I-Y(B)` (capacitive touch, black case)

This firmware is written for the capacitive-touch variant (`ESP32-2424S012C-I` family) using:

- MCU module: ESP32-C3-MINI-1U (RISC-V single-core, up to 160 MHz)
- Display: 1.28 inch 240×240 IPS, GC9A01
- Touch controller: CST816S (I2C)

From `hardware.pdf`:

- Flash: 4MB
- Operating voltage: 5V
- Typical current draw: ~100mA
- Board size: 38.5mm × 37.0mm

### Pin Mapping

Display (SPI2):

- MOSI: GPIO 7
- SCLK: GPIO 6
- CS: GPIO 10
- DC: GPIO 2
- RST: GPIO 3
- BL: GPIO 11

Touch (CST816S, I2C):

- SDA: GPIO 4
- SCL: GPIO 5
- INT: GPIO 0
- RST: GPIO 1

## Software Stack

- Arduino framework (PlatformIO)
- LVGL 8.x
- LovyanGFX
- ArduinoJson
- HTTPClient / WiFiClientSecure
- TJpg_Decoder (JPEG decoding for map background)
- WiFiManager (captive portal)
- FreeRTOS (built into ESP32 Arduino core)

## Data Sources

| Source | Purpose | Protocol |
|--------|---------|----------|
| `api.adsb.lol/v2/lat/.../lon/.../dist/...` | Live aircraft positions | Plain HTTP |
| `api.adsbdb.com/v0/callsign/...` | Route + IATA flight number lookup | HTTPS |
| `maps.googleapis.com/maps/api/staticmap` | Boot-time map background (120×120 JPEG) | HTTP |

Notes:

- adsb.lol has no documented rate limit but throttles by IP under heavy load. With multiple devices on the same network, consider increasing `OPENSKY_FETCH_PERIOD_MS`.
- Route data availability depends on callsign coverage. Private/GA/balloon traffic often has no route data.
- The Google Static Maps API key must be enabled for the Static Maps API in the Google Cloud Console.

## Project Structure

```text
src/
  main.cpp
  config.h
  lv_conf.h
  display/
    display.cpp / .h        — LovyanGFX + LVGL init
    ui.cpp / .h             — Radar UI (rings, sweep, labels, WiFi panel)
    aircraft_layer.cpp / .h — Aircraft icons, labels, detail panel
    map_background.cpp / .h — Boot-time map fetch, decode, LVGL install
  model/
    flight.h                — FlightData struct
  network/
    flight_data.cpp / .h    — ADS-B fetch + route/IATA cache
    wifi_manager.cpp / .h   — WiFiManager wrapper
    ntp.cpp / .h            — NTP sync
  touch/
    cst816s.cpp / .h        — CST816S touch driver
platformio.ini
```

## Architecture

The firmware uses two FreeRTOS tasks:

- **Display task** — owns all LVGL calls; handles touch, sweep animation, aircraft layer updates, and map installation
- **Network task** — WiFi connect → map fetch → NTP sync → ADS-B polling loop; handles tap-triggered route requests between polling cycles

Shared flight data is protected with a FreeRTOS mutex.

### Map background flow

1. After WiFi connects, network task fetches a 120×120 JPEG from Google Static Maps over HTTP
2. TJpg_Decoder decodes the JPEG into an RGB565 pixel buffer (28KB)
3. Pixel brightness is reduced to 20% in-place (avoids per-frame opacity blending)
4. Display task installs the buffer as an LVGL image object at the bottom of the z-order
5. LVGL zooms it 2× to fill the 240×240 screen

### Memory notes

- ESP32-C3 has 320KB DRAM, no PSRAM
- Map pixel buffer: 28KB (120×120×2), allocated before WiFi starts
- JPEG decode buffer: 20KB
- LVGL draw buffers: 2×11KB (double-buffered, 24-line)
- TLS context (route lookups): ~36KB peak, freed via `client.stop()` after each call

## Getting Started

### 1. Requirements

- VS Code + PlatformIO extension
- USB connection to the ESP32-C3 board
- Google Cloud API key with Static Maps API enabled (for map background)

### 2. Build

```bash
/home/$USER/.platformio/penv/bin/pio run --environment esp32-c3-devkitm-1
```

### 3. Upload

```bash
/home/$USER/.platformio/penv/bin/pio run --target upload --environment esp32-c3-devkitm-1
```

### 4. Monitor

```bash
/home/$USER/.platformio/penv/bin/pio device monitor --environment esp32-c3-devkitm-1
```

## Configuration

Edit `src/config.h` before flashing:

| Define | Description |
|--------|-------------|
| `WIFI_SSID` / `WIFI_PASSWORD` | Static credentials (optional; WiFiManager portal used if blank) |
| `HOME_LAT` / `HOME_LON` | Radar center coordinates |
| `RADAR_RADIUS_KM` | Radar coverage radius |
| `OPENSKY_FETCH_PERIOD_MS` | ADS-B polling interval (default 5000 ms) |
| `OPENSKY_HTTP_TIMEOUT_MS` | HTTP timeout for ADS-B and route requests |
| `MAP_FETCH_ON_BOOT` | Enable/disable map background (1/0) |
| `GOOGLE_STATIC_MAPS_API_KEY` | Google Static Maps API key |

**Important:** do not commit real Wi-Fi credentials or API keys to public repositories.

## UI Behavior

When an aircraft is tapped:

1. Details panel opens immediately with callsign and current alt/speed
2. Route lookup starts if dep/arr/IATA is not yet cached
3. Panel shows `Retrieving...` while waiting
4. When data arrives: ICAO callsign shown left, IATA flight number right; route on second line
5. If no data by timeout (15 s), panel shows `N/A`
6. Panel auto-closes after 7 seconds once route data is shown

## Troubleshooting

- No aircraft shown:
  - Check Wi-Fi status and home coordinates
  - Verify HTTPS access from your network
- Touch seems rotated/inverted:
  - Adjust TOUCH_SWAP_XY / TOUCH_INVERT_X / TOUCH_INVERT_Y in src/config.h
- Serial monitor cannot open port:
  - Close any other app or monitor session using the same serial device

## Roadmap Ideas

- Better vector icons for aircraft kinds
- Optional trails/history
- Airport overlays from live data
- Credential storage for private Wi-Fi config at runtime

## License

Add your preferred license (MIT, Apache-2.0, etc.) in a LICENSE file.

## Disclaimer

This project is community-built and is not affiliated with any flight-tracking provider.
