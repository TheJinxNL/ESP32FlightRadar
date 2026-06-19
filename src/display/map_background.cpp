#include "map_background.h"
#include "../config.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>

namespace {

static lv_color_t *s_map_pixels = nullptr;
static lv_img_dsc_t s_map_dsc;
static lv_obj_t *s_map_obj = nullptr;
static volatile bool s_map_ready = false;
static char s_map_status[64] = "MAP: idle";

static lv_color_t *s_decode_target = nullptr;
static bool s_decode_wrote_any = false;

constexpr float K_DEG_TO_RAD = 0.01745329252f;

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int compute_google_zoom() {
    const float lat_rad = HOME_LAT * K_DEG_TO_RAD;
    const float meters_per_pixel = (RADAR_RADIUS_KM * 1000.0f) / 110.0f;
    const float denom = meters_per_pixel > 0.1f ? meters_per_pixel : 0.1f;
    const float zoom_f = log2f((cosf(lat_rad) * 156543.03392f) / denom);
    const int zoom = (int)lroundf(zoom_f);
    return clamp_int(zoom, 1, 21);
}

static void set_map_status(const char *text) {
    if (text == nullptr) {
        return;
    }
    strncpy(s_map_status, text, sizeof(s_map_status) - 1);
    s_map_status[sizeof(s_map_status) - 1] = '\0';
}

static String build_static_map_url() {
    String url = "http://maps.googleapis.com/maps/api/staticmap?";
    url += "center=";
    url += String(HOME_LAT, 6);
    url += ",";
    url += String(HOME_LON, 6);
    url += "&zoom=";
    url += String(compute_google_zoom());
    url += "&size=120x120";
    url += "&scale=1";
    url += "&maptype=roadmap";
    url += "&format=jpg-baseline";
    url += "&key=";
    url += GOOGLE_STATIC_MAPS_API_KEY;
    return url;
}

static bool jpg_to_rgb565_cb(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    constexpr int MAP_IMG_W = 120;
    constexpr int MAP_IMG_H = 120;
    if (s_decode_target == nullptr || bitmap == nullptr) {
        return false;
    }

    if (x >= MAP_IMG_W || y >= MAP_IMG_H) {
        return true;
    }

    const uint16_t copy_w = (uint16_t)((x + w > MAP_IMG_W) ? (MAP_IMG_W - x) : w);
    const uint16_t copy_h = (uint16_t)((y + h > MAP_IMG_H) ? (MAP_IMG_H - y) : h);

    for (uint16_t row = 0; row < copy_h; ++row) {
        uint16_t *dst = (uint16_t *)&s_decode_target[(y + row) * MAP_IMG_W + x];
        uint16_t *src = &bitmap[row * w];
        memcpy(dst, src, copy_w * sizeof(uint16_t));
    }

    s_decode_wrote_any = true;
    return true;
}

} // namespace

bool map_background_fetch_once() {
#if !MAP_FETCH_ON_BOOT
    set_map_status("MAP: disabled");
    return false;
#else
    if (s_map_ready) {
        set_map_status("MAP: ready");
        return s_map_ready;
    }

    if (GOOGLE_STATIC_MAPS_API_KEY[0] == '\0') {
        set_map_status("MAP: key empty");
        Serial.println("[map] skipped: GOOGLE_STATIC_MAPS_API_KEY is empty");
        return false;
    }

    HTTPClient http;
    const String url = build_static_map_url();
    set_map_status("MAP: requesting");
    Serial.printf("[map] GET %s\n", url.c_str());

    if (!http.begin(url)) {
        set_map_status("MAP: begin failed");
        Serial.println("[map] begin failed");
        return false;
    }

    http.setReuse(false);
    http.useHTTP10(true);
    http.addHeader("Connection", "close");
    http.setTimeout(MAP_HTTP_TIMEOUT_MS);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        snprintf(s_map_status, sizeof(s_map_status), "MAP HTTP %d", code);
        Serial.printf("[map] HTTP %d\n", code);
        http.end();
        return false;
    }

    int content_len = http.getSize();
    Serial.printf("[map] content_len=%d\n", content_len);
    if (content_len > MAP_MAX_JPEG_BYTES) {
        set_map_status("MAP: too large");
        Serial.printf("[map] too large: %d bytes\n", content_len);
        http.end();
        return false;
    }

    // Allocate pixel buffer first while heap is unfragmented (larger block).
    // 120x120 @ RGB565 = 28800 bytes; LVGL zooms 2x to fill 240x240.
    constexpr int MAP_IMG_W = 120;
    constexpr int MAP_IMG_H = 120;
    const size_t pixel_bytes = (size_t)MAP_IMG_W * MAP_IMG_H * sizeof(lv_color_t);
    Serial.printf("[map] allocating %u bytes for pixels\n", (unsigned)pixel_bytes);
    s_map_pixels = (lv_color_t *)heap_caps_malloc(pixel_bytes, MALLOC_CAP_8BIT);
    if (s_map_pixels == nullptr) {
        set_map_status("MAP: no pixel heap");
        Serial.println("[map] no heap for map pixels");
        http.end();
        return false;
    }
    Serial.printf("[map] pixels allocated at %p\n", (void*)s_map_pixels);

    uint8_t *jpg_data = (uint8_t *)heap_caps_malloc(MAP_MAX_JPEG_BYTES, MALLOC_CAP_8BIT);
    if (jpg_data == nullptr) {
        set_map_status("MAP: no JPEG heap");
        Serial.println("[map] no heap for JPEG buffer");
        free(s_map_pixels);
        s_map_pixels = nullptr;
        http.end();
        return false;
    }

    Stream *stream = http.getStreamPtr();
    size_t used = 0;
    uint32_t last_progress_ms = millis();

    while ((http.connected() || stream->available() > 0) && (content_len > 0 || content_len == -1)) {
        size_t avail = stream->available();
        if (avail == 0) {
            if ((millis() - last_progress_ms) > (uint32_t)MAP_HTTP_TIMEOUT_MS) {
                set_map_status("MAP: stream timeout");
                Serial.println("[map] stream timeout waiting for payload bytes");
                break;
            }
            delay(1);
            continue;
        }

        size_t can_read = avail;
        if (content_len > 0 && can_read > (size_t)content_len) {
            can_read = (size_t)content_len;
        }
        if (used + can_read > MAP_MAX_JPEG_BYTES) {
            can_read = MAP_MAX_JPEG_BYTES - used;
        }

        if (can_read == 0) {
            set_map_status("MAP: JPEG too large");
            Serial.println("[map] JPEG exceeds configured buffer");
            free(jpg_data);
            free(s_map_pixels);
            s_map_pixels = nullptr;
            http.end();
            return false;
        }

        const size_t got = stream->readBytes(jpg_data + used, can_read);
        if (got == 0) {
            break;
        }

        used += got;
        last_progress_ms = millis();
        if (content_len > 0) {
            content_len -= (int)got;
        }
    }

    http.end();

    if (used == 0) {
        set_map_status("MAP: empty payload");
        Serial.println("[map] empty JPEG payload");
        free(jpg_data);
        free(s_map_pixels);
        s_map_pixels = nullptr;
        return false;
    }

    memset(s_map_pixels, 0, pixel_bytes);
    Serial.println("[map] pixels initialized to black");

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(jpg_to_rgb565_cb);

    s_decode_target = s_map_pixels;
    s_decode_wrote_any = false;
    Serial.printf("[map] starting JPEG decode (%u bytes payload)\n", (unsigned)used);
    const bool decode_ok = (TJpgDec.drawJpg(0, 0, jpg_data, used) == 0);
    s_decode_target = nullptr;

    free(jpg_data);

    Serial.printf("[map] decode result: ok=%d wrote_any=%d\n", decode_ok ? 1 : 0, s_decode_wrote_any ? 1 : 0);
    if (!decode_ok || !s_decode_wrote_any) {
        set_map_status("MAP: decode failed");
        Serial.printf("[map] JPEG decode failed\n");
        free(s_map_pixels);
        s_map_pixels = nullptr;
        return false;
    }

    // Bake 20% brightness into pixel buffer once — avoids per-frame opacity blending.
    for (size_t pi = 0; pi < (size_t)MAP_IMG_W * MAP_IMG_H; ++pi) {
        uint16_t px = s_map_pixels[pi].full;
        uint16_t r = ((px >> 11) & 0x1F) * 51 / 255;
        uint16_t g = ((px >> 5)  & 0x3F) * 51 / 255;
        uint16_t b = ( px        & 0x1F) * 51 / 255;
        s_map_pixels[pi].full = (uint16_t)((r << 11) | (g << 5) | b);
    }

    Serial.printf("[map] initializing image descriptor (CF=%u w=%u h=%u)\n", LV_IMG_CF_TRUE_COLOR, MAP_IMG_W, MAP_IMG_H);
    s_map_dsc.header.always_zero = 0;
    s_map_dsc.header.reserved = 0;
    s_map_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_map_dsc.header.w = MAP_IMG_W;
    s_map_dsc.header.h = MAP_IMG_H;
    s_map_dsc.data_size = pixel_bytes;
    s_map_dsc.data = (const uint8_t *)s_map_pixels;

    s_map_ready = true;
    set_map_status("MAP: ready");
    Serial.printf("[map] ready (%u bytes JPEG decoded to %u bytes pixels)\n", (unsigned)used, (unsigned)pixel_bytes);
    return true;
#endif
}

const char *map_background_last_status() {
    return s_map_status;
}

void map_background_try_install() {
    Serial.printf("[map] try_install: s_map_ready=%d s_map_obj=%p\n", s_map_ready ? 1 : 0, (void*)s_map_obj);
    if (!s_map_ready || s_map_obj != nullptr) {
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    if (scr == nullptr) {
        Serial.println("[map] active screen is null");
        return;
    }

    Serial.printf("[map] creating image on screen %p\n", (void*)scr);
    s_map_obj = lv_img_create(scr);
    if (s_map_obj == nullptr) {
        Serial.println("[map] ERROR: lv_img_create returned null");
        return;
    }

    Serial.printf("[map] image object created at %p, setting source\n", (void*)s_map_obj);
    lv_img_set_src(s_map_obj, &s_map_dsc);
    Serial.println("[map] image source set");

    lv_img_set_zoom(s_map_obj, 512);  // 512 = 2x in LVGL (256 = 1x)
    lv_obj_center(s_map_obj);
    Serial.println("[map] image centered at 2x zoom");

    lv_obj_clear_flag(s_map_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_opa(s_map_obj, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_move_background(s_map_obj);
    Serial.println("[map] moved to background");

    Serial.printf("[map] installed: object=%p width=%u height=%u\n", 
                  (void*)s_map_obj, 
                  lv_obj_get_width(s_map_obj), 
                  lv_obj_get_height(s_map_obj));
}
