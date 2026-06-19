#include "ui.h"
#include "../config.h"
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace {

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 240;
constexpr int CENTER_X = SCREEN_W / 2;
constexpr int CENTER_Y = SCREEN_H / 2;
constexpr int OUTER_RADIUS = 110;

constexpr int SWEEP_TIMER_MS = 33;     // ~30 FPS
constexpr int SWEEP_PERIOD_MS = 12000;  // full rotation period (50% slower)
constexpr int TRAIL_COUNT = 1;
constexpr int TRAIL_STEP_DEG = 1;

static lv_obj_t *s_sweep_lines[TRAIL_COUNT];
static lv_point_t s_sweep_points[TRAIL_COUNT][2];
static float s_sweep_angle_deg = 0.0f;
static uint32_t s_last_tick_ms = 0;
static lv_obj_t *s_status_label = nullptr;
static portMUX_TYPE s_status_mux = portMUX_INITIALIZER_UNLOCKED;
static char s_pending_status[32] = "RADAR SWEEP";
static volatile bool s_status_dirty = true;
static lv_obj_t *s_wifi_panel = nullptr;
static volatile bool s_wifi_portal_active_pending = false;
static volatile bool s_wifi_panel_dirty = true;

struct AirportMarker {
    const char *icao;
    float lat;
    float lon;
};

// Nearby airports around Best, NL. Markers outside current radius are skipped.
static const AirportMarker s_airports[] = {
    {"EHEH", 51.4501f, 5.3745f}, // Eindhoven
    {"EHBD", 51.2553f, 5.6014f}, // Budel
    {"EHVK", 51.6564f, 5.7086f}, // Volkel
    {"EHGR", 51.5674f, 4.9318f}, // Gilze-Rijen
};

inline float deg_to_rad(float deg) {
    return deg * 0.01745329252f;
}

static lv_point_t polar_to_screen(float angle_deg, int radius) {
    // 0 deg at North, increasing clockwise.
    float plot_deg = angle_deg - 90.0f;
    float rad = deg_to_rad(plot_deg);

    lv_point_t p;
    p.x = (lv_coord_t)(CENTER_X + (int)lroundf(cosf(rad) * radius));
    p.y = (lv_coord_t)(CENTER_Y + (int)lroundf(sinf(rad) * radius));
    return p;
}

static lv_point_t geo_to_screen(float lat, float lon) {
    const float lat_off = lat - HOME_LAT;
    const float lon_off = lon - HOME_LON;

    const float lat_km = lat_off * 111.0f;
    const float lon_km = lon_off * 111.0f * cosf(HOME_LAT * 0.01745329252f);

    const float r_km = sqrtf(lat_km * lat_km + lon_km * lon_km);
    const float r_px = (r_km / RADAR_RADIUS_KM) * OUTER_RADIUS;

    float angle_deg = atan2f(lon_km, lat_km) * 57.29577951f;
    if (angle_deg < 0.0f) {
        angle_deg += 360.0f;
    }

    return polar_to_screen(angle_deg, (int)r_px);
}

static void create_airport_markers(lv_obj_t *parent) {
    const lv_color_t airport_red = lv_color_hex(0xFF2A2A);

    for (size_t i = 0; i < (sizeof(s_airports) / sizeof(s_airports[0])); ++i) {
        const float lat_off = s_airports[i].lat - HOME_LAT;
        const float lon_off = s_airports[i].lon - HOME_LON;
        const float lat_km = lat_off * 111.0f;
        const float lon_km = lon_off * 111.0f * cosf(HOME_LAT * 0.01745329252f);
        const float dist_km = sqrtf(lat_km * lat_km + lon_km * lon_km);
        if (dist_km > RADAR_RADIUS_KM) {
            continue;
        }

        lv_point_t p = geo_to_screen(s_airports[i].lat, s_airports[i].lon);
        lv_obj_t *dot = lv_obj_create(parent);
        lv_obj_set_size(dot, 4, 4);
        lv_obj_set_pos(dot, p.x - 2, p.y - 2);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, airport_red, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }
}

static void sweep_timer_cb(lv_timer_t * /*t*/) {
    uint32_t now_ms = lv_tick_get();
    if (s_last_tick_ms == 0) {
        s_last_tick_ms = now_ms;
    }

    uint32_t dt_ms = now_ms - s_last_tick_ms;
    s_last_tick_ms = now_ms;

    const float deg_per_ms = 360.0f / (float)SWEEP_PERIOD_MS;
    s_sweep_angle_deg += deg_per_ms * (float)dt_ms;
    while (s_sweep_angle_deg >= 360.0f) {
        s_sweep_angle_deg -= 360.0f;
    }

    for (int i = 0; i < TRAIL_COUNT; ++i) {
        float angle = s_sweep_angle_deg - (float)(i * TRAIL_STEP_DEG);
        while (angle < 0.0f) {
            angle += 360.0f;
        }

        lv_point_t end = polar_to_screen(angle, OUTER_RADIUS - 2);

        s_sweep_points[i][0].x = CENTER_X;
        s_sweep_points[i][0].y = CENTER_Y;
        s_sweep_points[i][1] = end;

        lv_line_set_points(s_sweep_lines[i], s_sweep_points[i], 2);

        lv_obj_set_style_line_width(s_sweep_lines[i], (i == 0) ? 3 : 2, LV_PART_MAIN);
    }

    if (s_status_label != nullptr && s_status_dirty) {
        char buf[sizeof(s_pending_status)];
        taskENTER_CRITICAL(&s_status_mux);
        strncpy(buf, s_pending_status, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        s_status_dirty = false;
        taskEXIT_CRITICAL(&s_status_mux);
        lv_label_set_text(s_status_label, buf);
    }

    if (s_wifi_panel != nullptr && s_wifi_panel_dirty) {
        bool active = false;
        taskENTER_CRITICAL(&s_status_mux);
        active = s_wifi_portal_active_pending;
        s_wifi_panel_dirty = false;
        taskEXIT_CRITICAL(&s_status_mux);

        if (active) {
            lv_obj_clear_flag(s_wifi_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_wifi_panel);
        } else {
            lv_obj_add_flag(s_wifi_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void create_ring(lv_obj_t *parent, int radius_px, lv_color_t color) {
    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_set_size(ring, radius_px * 2, radius_px * 2);
    lv_obj_set_pos(ring, CENTER_X - radius_px, CENTER_Y - radius_px);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, color, LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 1, LV_PART_MAIN);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace

void ui_init() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);

    lv_color_t green_main = lv_color_hex(0x00FF66);
    lv_color_t green_mid  = lv_color_hex(0x00AA44);
    lv_color_t green_dim  = lv_color_hex(0x006633);

    // Radar frame and rings for 10/25/50 km.
    create_ring(scr, 110, lv_color_hex(0x00AA44));  // green_mid
    create_ring(scr, 55,  lv_color_hex(0x006633));  // green_dim
    create_ring(scr, 22,  lv_color_hex(0x006633));  // green_dim
    create_airport_markers(scr);

    // Crosshair.
    static lv_point_t hline[] = { {10, CENTER_Y}, {230, CENTER_Y} };
    static lv_point_t vline[] = { {CENTER_X, 10}, {CENTER_X, 230} };

    lv_obj_t *line_h = lv_line_create(scr);
    lv_line_set_points(line_h, hline, 2);
    lv_obj_set_style_line_color(line_h, lv_color_hex(0x006633), LV_PART_MAIN);
    lv_obj_set_style_line_opa(line_h, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(line_h, 1, LV_PART_MAIN);

    lv_obj_t *line_v = lv_line_create(scr);
    lv_line_set_points(line_v, vline, 2);
    lv_obj_set_style_line_color(line_v, lv_color_hex(0x006633), LV_PART_MAIN);
    lv_obj_set_style_line_opa(line_v, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(line_v, 1, LV_PART_MAIN);

    // Cardinal labels.
    lv_obj_t *label_n = lv_label_create(scr);
    lv_label_set_text(label_n, "N");
    lv_obj_set_style_text_color(label_n, green_main, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_n, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label_n, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *label_e = lv_label_create(scr);
    lv_label_set_text(label_e, "E");
    lv_obj_set_style_text_color(label_e, green_main, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_e, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label_e, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t *label_s = lv_label_create(scr);
    lv_label_set_text(label_s, "S");
    lv_obj_set_style_text_color(label_s, green_main, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_s, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label_s, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_t *label_w = lv_label_create(scr);
    lv_label_set_text(label_w, "W");
    lv_obj_set_style_text_color(label_w, green_main, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_w, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label_w, LV_ALIGN_LEFT_MID, 8, 0);

    // Outer range label.
    lv_obj_t *range = lv_label_create(scr);
    lv_label_set_text(range, "50 km");
    lv_obj_set_style_text_color(range, green_dim, LV_PART_MAIN);
    lv_obj_set_style_text_font(range, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(range, LV_ALIGN_BOTTOM_RIGHT, -14, -16);

    // Sweep lines with fading trail.
    for (int i = 0; i < TRAIL_COUNT; ++i) {
        s_sweep_lines[i] = lv_line_create(scr);
        lv_obj_set_style_line_color(s_sweep_lines[i], lv_color_hex(0x00D254), LV_PART_MAIN);  // green_main @ ~82%
        lv_obj_set_style_line_opa(s_sweep_lines[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(s_sweep_lines[i], true, LV_PART_MAIN);
        s_sweep_points[i][0].x = CENTER_X;
        s_sweep_points[i][0].y = CENTER_Y;
        s_sweep_points[i][1].x = CENTER_X;
        s_sweep_points[i][1].y = CENTER_Y - OUTER_RADIUS;
        lv_line_set_points(s_sweep_lines[i], s_sweep_points[i], 2);
    }

    // Center dot.
    lv_obj_t *center = lv_obj_create(scr);
    lv_obj_set_size(center, 6, 6);
    lv_obj_set_pos(center, CENTER_X - 3, CENTER_Y - 3);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(center, green_main, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(center, 0, LV_PART_MAIN);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);

    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "RADAR SWEEP");
    lv_obj_set_style_text_color(s_status_label, green_dim, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 26);

    // Captive-portal instructions use the same style language as flight details.
    s_wifi_panel = lv_obj_create(scr);
    lv_obj_set_size(s_wifi_panel, 190, 78);
    lv_obj_align(s_wifi_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_wifi_panel, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_wifi_panel, lv_color_hex(0x001508), LV_PART_MAIN);  // 0x001A0A @ 80%
    lv_obj_set_style_bg_opa(s_wifi_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_wifi_panel, lv_color_hex(0x00993D), LV_PART_MAIN);  // 0x00AA44 @ 90%
    lv_obj_set_style_border_opa(s_wifi_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_wifi_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_wifi_panel, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_wifi_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_wifi_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *wifi_title = lv_label_create(s_wifi_panel);
    lv_label_set_text(wifi_title, "WiFi Setup");
    lv_obj_set_style_text_color(wifi_title, lv_color_hex(0x00FF66), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *wifi_line1 = lv_label_create(s_wifi_panel);
    lv_label_set_text_fmt(wifi_line1, "AP: %s", WIFI_MANAGER_AP_NAME);
    lv_obj_set_style_text_color(wifi_line1, lv_color_hex(0x00AA44), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_line1, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(wifi_line1, wifi_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);

    lv_obj_t *wifi_line2 = lv_label_create(s_wifi_panel);
    lv_label_set_text(wifi_line2, "Open: 192.168.4.1");
    lv_obj_set_style_text_color(wifi_line2, lv_color_hex(0x00AA44), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_line2, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(wifi_line2, wifi_line1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_t *wifi_line3 = lv_label_create(s_wifi_panel);
    if (WIFI_MANAGER_AP_PASSWORD[0] != '\0') {
        lv_label_set_text(wifi_line3, "Enter AP password");
    } else {
        lv_label_set_text(wifi_line3, "AP has no password");
    }
    lv_obj_set_style_text_color(wifi_line3, lv_color_hex(0x00AA44), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_line3, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(wifi_line3, wifi_line2, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    // Start at least one frame immediately, then animate.
    sweep_timer_cb(nullptr);
    lv_timer_create(sweep_timer_cb, SWEEP_TIMER_MS, nullptr);
}

void ui_set_network_status(const char *status_text) {
    if (status_text == nullptr) {
        return;
    }

    taskENTER_CRITICAL(&s_status_mux);
    strncpy(s_pending_status, status_text, sizeof(s_pending_status) - 1);
    s_pending_status[sizeof(s_pending_status) - 1] = '\0';
    s_status_dirty = true;
    taskEXIT_CRITICAL(&s_status_mux);
}

void ui_set_wifi_portal_active(bool active) {
    taskENTER_CRITICAL(&s_status_mux);
    s_wifi_portal_active_pending = active;
    s_wifi_panel_dirty = true;
    taskEXIT_CRITICAL(&s_status_mux);
}
