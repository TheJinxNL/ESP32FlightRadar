#include "flight_data.h"
#include "../config.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <string.h>

namespace {

constexpr uint32_t ROUTE_CACHE_TTL_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t ROUTE_RETRY_MS = 90UL * 1000UL;
constexpr size_t ROUTE_CACHE_SIZE = MAX_FLIGHTS;
constexpr size_t ROUTE_FETCHES_PER_CYCLE = 1;

struct RouteCacheEntry {
    char icao24[9];
    char departure[5];
    char arrival[5];
    char callsign_iata[9];
    uint32_t updated_ms;
    bool valid;
};

static RouteCacheEntry s_route_cache[ROUTE_CACHE_SIZE];

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static String build_url() {
    const float lat = HOME_LAT;
    const float lon = HOME_LON;
    const int dist_km = (int)RADAR_RADIUS_KM;

    // adsb.lol supports plain HTTP — no TLS overhead on the frequent polling call.
    String url = "http://api.adsb.lol/v2/lat/";
    url += String(lat, 5);
    url += "/lon/";
    url += String(lon, 5);
    url += "/dist/";
    url += String(dist_km);
    return url;
}

static String build_route_url(const char *callsign) {
    String url = "https://api.adsbdb.com/v0/callsign/";
    // adsbdb expects uppercase callsign with no whitespace
    for (const char *p = callsign; *p; ++p) {
        const char c = *p;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            continue;
        }
        url += (char)toupper((unsigned char)c);
    }
    return url;
}

static void safe_copy(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';

    size_t start = 0;
    while (dst[start] == ' ' || dst[start] == '\n' || dst[start] == '\r' || dst[start] == '\t') {
        ++start;
    }

    if (start > 0) {
        memmove(dst, dst + start, strlen(dst + start) + 1);
    }

    size_t len = strlen(dst);
    while (len > 0) {
        const char c = dst[len - 1];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            dst[len - 1] = '\0';
            --len;
        } else {
            break;
        }
    }
}

static bool starts_with(const char *s, const char *prefix) {
    if (s == nullptr || prefix == nullptr) {
        return false;
    }
    while (*prefix) {
        if (*s != *prefix) {
            return false;
        }
        ++s;
        ++prefix;
    }
    return true;
}

static void infer_kind(const char *type_code, const char *desc, char *out_kind, size_t out_kind_size) {
    if (out_kind == nullptr || out_kind_size == 0) {
        return;
    }

    char type_up[12] = {0};
    size_t i = 0;
    if (type_code != nullptr) {
        for (; type_code[i] && i < sizeof(type_up) - 1; ++i) {
            type_up[i] = (char)toupper((unsigned char)type_code[i]);
        }
    }

    if (strstr(type_up, "BALL") != nullptr) {
        safe_copy(out_kind, out_kind_size, "BALLOON");
        return;
    }

    if (starts_with(type_up, "H") || starts_with(type_up, "R22") || starts_with(type_up, "R44") ||
        starts_with(type_up, "R66") || starts_with(type_up, "EC") || starts_with(type_up, "AS") ||
        starts_with(type_up, "BK") || starts_with(type_up, "AW") || starts_with(type_up, "B06") ||
        starts_with(type_up, "B47")) {
        safe_copy(out_kind, out_kind_size, "HELI");
        return;
    }

    char desc_up[48] = {0};
    i = 0;
    if (desc != nullptr) {
        for (; desc[i] && i < sizeof(desc_up) - 1; ++i) {
            desc_up[i] = (char)toupper((unsigned char)desc[i]);
        }
    }

    if (strstr(desc_up, "HELICOPTER") != nullptr) {
        safe_copy(out_kind, out_kind_size, "HELI");
        return;
    }
    if (strstr(desc_up, "BALLOON") != nullptr) {
        safe_copy(out_kind, out_kind_size, "BALLOON");
        return;
    }

    if (type_up[0] != '\0') {
        safe_copy(out_kind, out_kind_size, "PLANE");
    } else {
        safe_copy(out_kind, out_kind_size, "UNKNOWN");
    }
}

static int find_cache_slot(const char *icao24) {
    for (size_t i = 0; i < ROUTE_CACHE_SIZE; ++i) {
        if (strncmp(s_route_cache[i].icao24, icao24, sizeof(s_route_cache[i].icao24)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int find_oldest_cache_slot() {
    uint32_t oldest = UINT32_MAX;
    int slot = 0;
    for (size_t i = 0; i < ROUTE_CACHE_SIZE; ++i) {
        if (!s_route_cache[i].valid) {
            return (int)i;
        }
        if (s_route_cache[i].updated_ms < oldest) {
            oldest = s_route_cache[i].updated_ms;
            slot = (int)i;
        }
    }
    return slot;
}

static bool fetch_route_for_callsign(const char *callsign,
                                     char *dep, size_t dep_size,
                                     char *arr, size_t arr_size,
                                     char *cs_iata, size_t cs_iata_size) {
    if (callsign == nullptr || callsign[0] == '\0') {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    const String url = build_route_url(callsign);
    Serial.printf("[route] GET %s\n", url.c_str());
    if (!http.begin(client, url)) {
        return false;
    }

    http.setTimeout(OPENSKY_HTTP_TIMEOUT_MS);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[route] HTTP %d\n", code);
        http.end();
        client.stop();
        return false;
    }

    String payload = http.getString();
    http.end();
    client.stop();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        return false;
    }

    // adsbdb response includes both IATA and ICAO codes.
    // Prefer IATA (3-letter) for display, fallback to ICAO (4-letter).
    JsonObject flightroute = doc["response"]["flightroute"].as<JsonObject>();
    if (flightroute.isNull()) {
        return false;
    }

    const char *dep_iata = flightroute["origin"]["iata_code"].as<const char*>();
    const char *arr_iata = flightroute["destination"]["iata_code"].as<const char*>();
    const char *dep_icao = flightroute["origin"]["icao_code"].as<const char*>();
    const char *arr_icao = flightroute["destination"]["icao_code"].as<const char*>();

    const char *dep_raw = (dep_iata != nullptr && dep_iata[0] != '\0') ? dep_iata : dep_icao;
    const char *arr_raw = (arr_iata != nullptr && arr_iata[0] != '\0') ? arr_iata : arr_icao;

    safe_copy(dep, dep_size, (dep_raw != nullptr) ? dep_raw : "");
    safe_copy(arr, arr_size, (arr_raw != nullptr) ? arr_raw : "");

    if (cs_iata != nullptr && cs_iata_size > 0) {
        const char *iata = flightroute["callsign_iata"].as<const char*>();
        safe_copy(cs_iata, cs_iata_size, (iata != nullptr) ? iata : "");
    }

    return dep[0] != '\0' && arr[0] != '\0';
}

static bool cache_lookup(const char *icao24, char *dep, size_t dep_size, char *arr, size_t arr_size,
                         char *cs_iata, size_t cs_iata_size) {
    const int slot = find_cache_slot(icao24);
    if (slot < 0 || !s_route_cache[slot].valid) {
        return false;
    }

    const uint32_t now = millis();
    if ((now - s_route_cache[slot].updated_ms) > ROUTE_CACHE_TTL_MS) {
        return false;
    }

    safe_copy(dep, dep_size, s_route_cache[slot].departure);
    safe_copy(arr, arr_size, s_route_cache[slot].arrival);
    if (cs_iata != nullptr && cs_iata_size > 0) {
        safe_copy(cs_iata, cs_iata_size, s_route_cache[slot].callsign_iata);
    }
    return dep[0] != '\0' && arr[0] != '\0';
}

static void cache_store(const char *icao24, const char *dep, const char *arr, const char *cs_iata) {
    const int existing = find_cache_slot(icao24);
    const int slot = (existing >= 0) ? existing : find_oldest_cache_slot();

    safe_copy(s_route_cache[slot].icao24, sizeof(s_route_cache[slot].icao24), icao24);
    safe_copy(s_route_cache[slot].departure, sizeof(s_route_cache[slot].departure), dep);
    safe_copy(s_route_cache[slot].arrival, sizeof(s_route_cache[slot].arrival), arr);
    safe_copy(s_route_cache[slot].callsign_iata, sizeof(s_route_cache[slot].callsign_iata),
              (cs_iata != nullptr) ? cs_iata : "");
    s_route_cache[slot].updated_ms = millis();
    s_route_cache[slot].valid = true;
}

static void cache_mark_attempt(const char *icao24) {
    const int existing = find_cache_slot(icao24);
    const int slot = (existing >= 0) ? existing : find_oldest_cache_slot();

    safe_copy(s_route_cache[slot].icao24, sizeof(s_route_cache[slot].icao24), icao24);
    s_route_cache[slot].departure[0] = '\0';
    s_route_cache[slot].arrival[0] = '\0';
    s_route_cache[slot].updated_ms = millis() - (ROUTE_CACHE_TTL_MS - ROUTE_RETRY_MS);
    s_route_cache[slot].valid = false;
}

} // namespace

bool flight_data_fetch_flights(FlightData *out_flights, size_t out_capacity, size_t *out_count) {
    if (out_flights == nullptr || out_count == nullptr || out_capacity == 0) {
        return false;
    }

    *out_count = 0;

    HTTPClient http;
    const String url = build_url();
    Serial.printf("[adsb] GET %s\n", url.c_str());

    if (!http.begin(url)) {
        Serial.println("[adsb] http.begin failed");
        return false;
    }

    http.setTimeout(OPENSKY_HTTP_TIMEOUT_MS);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[adsbx] HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    Serial.printf("[adsb] payload size=%u bytes\n", (unsigned)payload.length());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[adsb] json error: %s\n", err.c_str());
        return false;
    }

    // API response: {"ac": [{"hex":"...", "flight":"...", "lat":..., "lon":..., "alt_baro":..., "gs":..., "track":...}, ...]}
    JsonArray ac = doc["ac"].as<JsonArray>();
    if (ac.isNull()) {
        Serial.println("[adsbx] no ac array");
        return true;
    }

    Serial.printf("[adsbx] parsing %u aircraft (capacity=%u)\n", (unsigned)ac.size(), (unsigned)out_capacity);

    size_t n = 0;
    for (JsonVariant v : ac) {
        if (n >= out_capacity) {
            break;
        }

        JsonObject obj = v.as<JsonObject>();
        if (obj.isNull()) {
            continue;
        }

        if (obj["lat"].isNull() || obj["lon"].isNull()) {
            continue;
        }

        FlightData f{};
        // Some feeds use "hex" while others use "icao".
        const char *hex = obj["hex"].isNull() ? nullptr : obj["hex"].as<const char*>();
        const char *icao = obj["icao"].isNull() ? nullptr : obj["icao"].as<const char*>();
        safe_copy(f.icao24, sizeof(f.icao24), (hex != nullptr) ? hex : ((icao != nullptr) ? icao : ""));
        safe_copy(f.callsign, sizeof(f.callsign), obj["flight"].isNull() ? "" : obj["flight"].as<const char*>());
        f.origin_country[0] = '\0';
        const char *type_code = obj["t"].isNull() ? nullptr : obj["t"].as<const char*>();
        const char *desc = obj["desc"].isNull() ? nullptr : obj["desc"].as<const char*>();
        safe_copy(f.type_code, sizeof(f.type_code), (type_code != nullptr) ? type_code : "");
        infer_kind(f.type_code, desc, f.kind, sizeof(f.kind));
        f.departure[0] = '\0';
        f.arrival[0] = '\0';

        f.lat = obj["lat"].as<float>();
        f.lon = obj["lon"].as<float>();
        // Altitude can be provided as alt_baro (ft) or alt (ft).
        float alt_ft = 0.0f;
        if (!obj["alt_baro"].isNull()) {
            alt_ft = obj["alt_baro"].as<float>();
        } else if (!obj["alt"].isNull()) {
            alt_ft = obj["alt"].as<float>();
        }
        f.altitude_m = alt_ft * 0.3048f;
        // Ground speed in knots, convert to m/s
        f.speed_mps = obj["gs"].isNull() ? 0.0f : (obj["gs"].as<float>() * 0.51444f);
        if (!obj["track"].isNull()) {
            f.heading_deg = obj["track"].as<float>();
        } else {
            f.heading_deg = obj["calc_track"].isNull() ? 0.0f : obj["calc_track"].as<float>();
        }

        // Keep route info stable across ADS-B refresh cycles.
        char dep_buf[5] = {0};
        char arr_buf[5] = {0};
        char iata_buf[9] = {0};
        if (cache_lookup(f.icao24, dep_buf, sizeof(dep_buf), arr_buf, sizeof(arr_buf), iata_buf, sizeof(iata_buf))) {
            safe_copy(f.departure, sizeof(f.departure), dep_buf);
            safe_copy(f.arrival, sizeof(f.arrival), arr_buf);
            safe_copy(f.callsign_iata, sizeof(f.callsign_iata), iata_buf);
        }

        out_flights[n++] = f;
    }

    *out_count = n;

    if (n > 0) {
        const FlightData &f = out_flights[0];
        Serial.printf("[adsb] flights=%u first=%s %s lat=%.4f lon=%.4f\n",
                      (unsigned)n,
                      f.icao24,
                      f.callsign,
                      f.lat,
                      f.lon);
    } else {
        Serial.println("[adsb] flights=0");
    }

    return true;
}

bool flight_data_fetch_route(const char *icao24, const char *callsign,
                         char *dep, size_t dep_size,
                         char *arr, size_t arr_size,
                         char *callsign_iata, size_t callsign_iata_size) {
    if (dep == nullptr || arr == nullptr || dep_size == 0 || arr_size == 0) {
        return false;
    }

    dep[0] = '\0';
    arr[0] = '\0';
    if (callsign_iata != nullptr && callsign_iata_size > 0) {
        callsign_iata[0] = '\0';
    }

    if (icao24 == nullptr || icao24[0] == '\0' ||
        callsign == nullptr || callsign[0] == '\0') {
        return false;
    }

    if (cache_lookup(icao24, dep, dep_size, arr, arr_size, callsign_iata, callsign_iata_size)) {
        return true;
    }

    char dep_buf[5] = {0};
    char arr_buf[5] = {0};
    char iata_buf[9] = {0};
    if (fetch_route_for_callsign(callsign, dep_buf, sizeof(dep_buf), arr_buf, sizeof(arr_buf),
                                 iata_buf, sizeof(iata_buf))) {
        cache_store(icao24, dep_buf, arr_buf, iata_buf);
        safe_copy(dep, dep_size, dep_buf);
        safe_copy(arr, arr_size, arr_buf);
        if (callsign_iata != nullptr && callsign_iata_size > 0) {
            safe_copy(callsign_iata, callsign_iata_size, iata_buf);
        }
        Serial.printf("[opensky] route %s: %s -> %s (iata=%s)\n", callsign, dep_buf, arr_buf, iata_buf);
        return true;
    } else {
        cache_mark_attempt(icao24);
        return false;
    }
}
