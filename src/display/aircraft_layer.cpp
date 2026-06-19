#include "aircraft_layer.h"
#include "../config.h"
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

// On-demand route fetch — defined in main.cpp
extern void set_route_request(const char *icao24, const char *callsign);

// Declare extern globals from main.cpp
extern FlightData g_flights[MAX_FLIGHTS];
extern size_t g_flight_count;
extern SemaphoreHandle_t g_flights_mutex;

namespace {

constexpr int CENTER_X = 120;
constexpr int CENTER_Y = 120;
constexpr int OUTER_RADIUS = 110;
constexpr int MAX_AIRCRAFT_OBJECTS = 32;
constexpr int AIRCRAFT_ICON_SIZE = 16;

static const uint8_t PLANE_ALPHA[16 * 16] = {
    0, 0, 1, 0, 20, 162, 95, 0, 2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 7, 227, 246, 34, 1, 2, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 3, 0, 130, 255, 175, 0, 3, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 3, 0, 64, 252, 255, 87, 0, 3, 0, 0, 0, 0, 0,
    1, 0, 3, 3, 2, 11, 222, 254, 225, 18, 3, 4, 2, 2, 2, 1,
    185, 107, 0, 0, 0, 0, 143, 255, 255, 149, 0, 0, 0, 0, 0, 0,
    181, 254, 85, 55, 63, 55, 125, 255, 253, 252, 94, 54, 60, 58, 58, 15,
    90, 255, 255, 255, 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 215,
    90, 255, 255, 255, 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 215,
    181, 254, 85, 55, 63, 56, 126, 255, 253, 252, 94, 54, 60, 58, 58, 15,
    185, 107, 0, 0, 0, 0, 143, 255, 255, 149, 0, 0, 0, 0, 0, 0,
    1, 0, 3, 3, 2, 11, 223, 254, 225, 18, 3, 4, 2, 2, 2, 1,
    0, 1, 0, 3, 0, 64, 252, 255, 87, 0, 3, 0, 0, 0, 0, 0,
    1, 0, 0, 3, 0, 130, 255, 175, 0, 3, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 7, 227, 246, 34, 1, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 20, 162, 95, 0, 2, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t BALLOON_ALPHA[16 * 16] = {
    0, 0, 0, 1, 0, 0, 42, 82, 83, 43, 0, 0, 1, 0, 0, 0,
    0, 0, 1, 0, 19, 161, 243, 255, 255, 243, 161, 21, 0, 1, 0, 0,
    0, 1, 1, 14, 201, 240, 251, 152, 150, 251, 240, 203, 15, 1, 1, 0,
    0, 3, 0, 130, 243, 80, 246, 59, 56, 246, 80, 241, 133, 0, 3, 0,
    1, 0, 4, 223, 132, 57, 252, 29, 28, 253, 61, 130, 225, 4, 0, 1,
    1, 0, 22, 243, 72, 72, 242, 16, 16, 242, 72, 73, 243, 22, 0, 1,
    1, 0, 9, 234, 103, 65, 248, 22, 22, 249, 68, 101, 235, 9, 0, 1,
    0, 2, 0, 172, 208, 58, 251, 47, 44, 251, 61, 206, 175, 0, 2, 0,
    0, 2, 0, 46, 246, 179, 242, 108, 104, 241, 179, 245, 45, 0, 2, 0,
    0, 0, 1, 0, 173, 255, 255, 235, 233, 255, 255, 167, 0, 1, 0, 0,
    0, 0, 3, 1, 96, 238, 116, 238, 237, 119, 241, 89, 1, 3, 0, 0,
    0, 0, 1, 0, 11, 61, 0, 146, 145, 0, 64, 11, 0, 1, 0, 0,
    0, 0, 0, 2, 0, 139, 156, 0, 0, 155, 138, 0, 2, 0, 0, 0,
    0, 0, 0, 3, 1, 127, 249, 32, 31, 249, 128, 1, 3, 0, 0, 0,
    0, 0, 0, 1, 0, 31, 232, 240, 240, 233, 33, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 49, 92, 92, 49, 0, 0, 0, 0, 0, 0,
};

static const uint8_t HELI_ALPHA[16 * 16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 91, 162, 151, 153, 154, 154, 147, 155, 155, 152, 158, 118, 45, 89, 0,
    0, 22, 39, 38, 38, 27, 45, 121, 23, 25, 32, 44, 34, 120, 100, 0,
    0, 0, 0, 0, 0, 37, 107, 203, 136, 145, 143, 137, 138, 204, 57, 0,
    0, 2, 0, 46, 158, 151, 131, 252, 255, 255, 112, 22, 12, 81, 110, 0,
    2, 0, 38, 165, 55, 0, 40, 252, 251, 252, 68, 0, 0, 3, 16, 0,
    3, 0, 146, 142, 64, 78, 150, 253, 252, 243, 34, 1, 2, 0, 0, 0,
    1, 1, 189, 255, 255, 255, 255, 255, 255, 152, 0, 1, 0, 0, 1, 0,
    1, 0, 117, 215, 221, 222, 222, 203, 125, 3, 0, 1, 0, 0, 0, 0,
    0, 90, 101, 62, 58, 57, 58, 44, 26, 37, 16, 0, 0, 0, 0, 0,
    0, 37, 118, 137, 144, 148, 148, 151, 153, 161, 65, 0, 2, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

struct AircraftObject {
    lv_obj_t *icon_root;
    lv_obj_t *icon_img;
    lv_obj_t *label;
    char label_text[16];
    char icao24[9];
    char callsign[9];
    char type_code[9];
    char kind[9];
    char icon_kind[9];
    char departure[5];
    char arrival[5];
    char callsign_iata[9];
    float altitude_m;
    float speed_mps;
    float heading_deg;
    int16_t screen_x;
    int16_t screen_y;
    uint32_t last_update_time_ms;
    uint32_t first_seen_time_ms;
};
static AircraftObject s_aircraft[MAX_AIRCRAFT_OBJECTS];
static int s_aircraft_count = 0;
static int s_selected_slot = -1;
static uint32_t s_selected_at_ms = 0;
static bool s_selected_waiting_route = false;
static bool s_selected_route_na = false;

static lv_obj_t *s_detail_panel = nullptr;
static lv_obj_t *s_detail_title = nullptr;
static lv_obj_t *s_detail_iata = nullptr;
static lv_obj_t *s_detail_line1 = nullptr;
static lv_obj_t *s_detail_line2 = nullptr;
static char s_detail_title_cache[24] = {0};
static char s_detail_iata_cache[9] = {0};
static char s_detail_line1_cache[48] = {0};
static char s_detail_line2_cache[64] = {0};

constexpr uint32_t DETAIL_AUTO_CLOSE_MS = 7000;
constexpr uint32_t DETAIL_ROUTE_WAIT_TIMEOUT_MS = 15000;
constexpr int TOUCH_SELECT_RADIUS_PX = 24;


inline float deg_to_rad(float deg) {
    return deg * 0.01745329252f;
}

static float distance_haversine(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371.0f;
    const float dLat = (lat2 - lat1) * 0.01745329252f;
    const float dLon = (lon2 - lon1) * 0.01745329252f;
    const float a = sinf(dLat * 0.5f) * sinf(dLat * 0.5f) +
                    cosf(lat1 * 0.01745329252f) * cosf(lat2 * 0.01745329252f) *
                    sinf(dLon * 0.5f) * sinf(dLon * 0.5f);
    const float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return R * c;
}

static lv_point_t polar_to_screen(float angle_deg, int radius) {
    float plot_deg = angle_deg - 90.0f;
    float rad = deg_to_rad(plot_deg);

    lv_point_t p;
    p.x = (lv_coord_t)(CENTER_X + (int)lroundf(cosf(rad) * radius));
    p.y = (lv_coord_t)(CENTER_Y + (int)lroundf(sinf(rad) * radius));
    return p;
}

static lv_point_t flight_to_screen(const FlightData &f) {
    const float lat_off = f.lat - HOME_LAT;
    const float lon_off = f.lon - HOME_LON;

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

static int find_aircraft_slot(const char *icao24) {
    for (int i = 0; i < s_aircraft_count; ++i) {
        if (strncmp(s_aircraft[i].icao24, icao24, sizeof(s_aircraft[i].icao24)) == 0) {
            return i;
        }
    }
    return -1;
}

static const char *normalized_icon_kind(const char *kind) {
    if (kind == nullptr || kind[0] == '\0') {
        return "PLANE";
    }
    if (strncmp(kind, "BALLOON", 7) == 0) {
        return "BALLOON";
    }
    if (strncmp(kind, "HELI", 4) == 0) {
        return "HELI";
    }
    return "PLANE";
}

static void make_img_dsc(lv_img_dsc_t *dsc, const uint8_t *data) {
    dsc->header.cf = LV_IMG_CF_ALPHA_8BIT;
    dsc->header.always_zero = 0;
    dsc->header.reserved = 0;
    dsc->header.w = AIRCRAFT_ICON_SIZE;
    dsc->header.h = AIRCRAFT_ICON_SIZE;
    dsc->data_size = AIRCRAFT_ICON_SIZE * AIRCRAFT_ICON_SIZE;
    dsc->data = data;
}

static lv_img_dsc_t s_plane_img;
static lv_img_dsc_t s_balloon_img;
static lv_img_dsc_t s_heli_img;

static bool obj_valid(lv_obj_t *obj) {
    return (obj != nullptr) && lv_obj_is_valid(obj);
}

static void delete_icon(AircraftObject &a) {
    if (obj_valid(a.icon_root)) {
        lv_obj_del(a.icon_root);
    }
    a.icon_root = nullptr;
    a.icon_img = nullptr;
}

static void style_icon_root(lv_obj_t *root) {
    lv_obj_set_size(root, AIRCRAFT_ICON_SIZE, AIRCRAFT_ICON_SIZE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(root, 0, LV_PART_MAIN);
}

static void create_aircraft_icon(AircraftObject &a, lv_obj_t *parent, const char *kind, lv_color_t color) {
    delete_icon(a);

    const char *icon_kind = normalized_icon_kind(kind);
    strncpy(a.icon_kind, icon_kind, sizeof(a.icon_kind) - 1);
    a.icon_kind[sizeof(a.icon_kind) - 1] = '\0';

    a.icon_root = lv_obj_create(parent);
    style_icon_root(a.icon_root);

    lv_obj_t *img = lv_img_create(a.icon_root);
    a.icon_img = img;
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(img, 0, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(img, color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);

    if (strncmp(a.icon_kind, "BALLOON", 7) == 0) {
        make_img_dsc(&s_balloon_img, BALLOON_ALPHA);
        lv_img_set_src(img, &s_balloon_img);
    } else if (strncmp(a.icon_kind, "HELI", 4) == 0) {
        make_img_dsc(&s_heli_img, HELI_ALPHA);
        lv_img_set_src(img, &s_heli_img);
    } else {
        make_img_dsc(&s_plane_img, PLANE_ALPHA);
        lv_img_set_src(img, &s_plane_img);
    }
}

static void copy_trimmed(char *dst, size_t dst_size, const char *src) {
    if (dst == nullptr || dst_size == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';

    // Trim leading/trailing whitespace so callsign-based lookups work reliably.
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

static void set_label_if_changed(lv_obj_t *label, char *cache, size_t cache_size, const char *text) {
    if (!obj_valid(label) || cache == nullptr || cache_size == 0 || text == nullptr) {
        return;
    }
    if (strncmp(cache, text, cache_size) == 0) {
        return;
    }
    strncpy(cache, text, cache_size - 1);
    cache[cache_size - 1] = '\0';
    lv_label_set_text(label, cache);
}


static void detail_panel_hide() {
    if (s_detail_panel != nullptr) {
        lv_obj_add_flag(s_detail_panel, LV_OBJ_FLAG_HIDDEN);
    }
    s_selected_slot = -1;
    s_selected_waiting_route = false;
    s_selected_route_na = false;
}

static void detail_panel_show_for_slot(int slot, uint32_t now_ms) {
    if (slot < 0 || slot >= s_aircraft_count || s_detail_panel == nullptr) {
        return;
    }

    const AircraftObject &a = s_aircraft[slot];

    char title_buf[24];
    if (a.callsign[0] != '\0') {
        snprintf(title_buf, sizeof(title_buf), "%s", a.callsign);
    } else {
        snprintf(title_buf, sizeof(title_buf), "%s", a.icao24);
    }

    char line1[48] = {0};
    const bool has_route = (a.departure[0] != '\0' && strncmp(a.departure, "----", 4) != 0
                         && a.arrival[0] != '\0' && strncmp(a.arrival, "----", 4) != 0);
    if (has_route) {
        snprintf(line1, sizeof(line1), "DEP %s  ARR %s", a.departure, a.arrival);
    } else {
        snprintf(line1, sizeof(line1), "Retrieving...");
    }

    char line2[64];
    snprintf(line2, sizeof(line2), "ALT %.0fm  SPD %.0fm/s", a.altitude_m, a.speed_mps);

    set_label_if_changed(s_detail_title, s_detail_title_cache, sizeof(s_detail_title_cache), title_buf);
    set_label_if_changed(s_detail_iata,  s_detail_iata_cache,  sizeof(s_detail_iata_cache),  a.callsign_iata[0] ? a.callsign_iata : "");
    set_label_if_changed(s_detail_line1, s_detail_line1_cache, sizeof(s_detail_line1_cache), line1);
    set_label_if_changed(s_detail_line2, s_detail_line2_cache, sizeof(s_detail_line2_cache), line2);
    lv_obj_clear_flag(s_detail_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_detail_panel);

    s_selected_slot = slot;
    s_selected_waiting_route = !has_route;
    s_selected_route_na = false;
    s_selected_at_ms = now_ms;
}

static int find_nearest_aircraft_slot(int16_t x, int16_t y, int max_radius_px) {
    int best_slot = -1;
    int best_d2 = max_radius_px * max_radius_px;

    for (int i = 0; i < s_aircraft_count; ++i) {
        const int dx = (int)s_aircraft[i].screen_x - (int)x;
        const int dy = (int)s_aircraft[i].screen_y - (int)y;
        const int d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) {
            best_d2 = d2;
            best_slot = i;
        }
    }

    return best_slot;
}

} // namespace

void aircraft_layer_init() {
    for (int i = 0; i < MAX_AIRCRAFT_OBJECTS; ++i) {
        s_aircraft[i].icon_root = nullptr;
        s_aircraft[i].icon_img = nullptr;
        s_aircraft[i].label = nullptr;
        s_aircraft[i].label_text[0] = '\0';
        s_aircraft[i].icao24[0] = '\0';
        s_aircraft[i].callsign[0] = '\0';
        s_aircraft[i].type_code[0] = '\0';
        s_aircraft[i].kind[0] = '\0';
        s_aircraft[i].icon_kind[0] = '\0';
        s_aircraft[i].departure[0] = '\0';
        s_aircraft[i].arrival[0] = '\0';
        s_aircraft[i].callsign_iata[0] = '\0';
        s_aircraft[i].altitude_m = 0.0f;
        s_aircraft[i].speed_mps = 0.0f;
        s_aircraft[i].heading_deg = 0.0f;
        s_aircraft[i].screen_x = -1000;
        s_aircraft[i].screen_y = -1000;
    }
    make_img_dsc(&s_plane_img, PLANE_ALPHA);
    make_img_dsc(&s_balloon_img, BALLOON_ALPHA);
    make_img_dsc(&s_heli_img, HELI_ALPHA);
    s_aircraft_count = 0;
    s_selected_slot = -1;

    lv_obj_t *scr = lv_scr_act();
    s_detail_panel = lv_obj_create(scr);
    lv_obj_set_size(s_detail_panel, 180, 54);
    lv_obj_align(s_detail_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_detail_panel, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_detail_panel, lv_color_hex(0x001A0A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_detail_panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_detail_panel, lv_color_hex(0x00AA44), LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_detail_panel, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_detail_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_detail_panel, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_detail_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_detail_panel, LV_OBJ_FLAG_HIDDEN);

    s_detail_title = lv_label_create(s_detail_panel);
    lv_obj_set_style_text_color(s_detail_title, lv_color_hex(0x00FF66), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_detail_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_detail_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_detail_iata = lv_label_create(s_detail_panel);
    lv_label_set_text(s_detail_iata, "");
    lv_obj_set_style_text_color(s_detail_iata, lv_color_hex(0x00FF66), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_detail_iata, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_detail_iata, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_detail_line1 = lv_label_create(s_detail_panel);
    lv_obj_set_style_text_color(s_detail_line1, lv_color_hex(0x00AA44), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_detail_line1, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(s_detail_line1, s_detail_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);

    s_detail_line2 = lv_label_create(s_detail_panel);
    lv_obj_set_style_text_color(s_detail_line2, lv_color_hex(0x00AA44), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_detail_line2, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align_to(s_detail_line2, s_detail_line1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
}

void aircraft_layer_handle_tap(int16_t x, int16_t y) {
    const uint32_t now_ms = lv_tick_get();
    int slot = find_nearest_aircraft_slot(x, y, TOUCH_SELECT_RADIUS_PX);
    if (slot < 0) {
        detail_panel_hide();
        return;
    }
    detail_panel_show_for_slot(slot, now_ms);
    // Kick off a flightplan fetch for this aircraft
    if (s_selected_waiting_route) {
        set_route_request(s_aircraft[slot].icao24, s_aircraft[slot].callsign);
    }
}

void aircraft_layer_update() {
    if (xSemaphoreTake(g_flights_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }

    const uint32_t now_ms = lv_tick_get();
    const lv_color_t dot_color = lv_color_hex(0x00FF66);
    const lv_color_t label_color = lv_color_hex(0x00AA44);

    int visible_count = 0;
    for (size_t i = 0; i < g_flight_count && visible_count < MAX_AIRCRAFT_OBJECTS; ++i) {
        const FlightData &f = g_flights[i];

        const float dist_km = distance_haversine(HOME_LAT, HOME_LON, f.lat, f.lon);
        if (dist_km > RADAR_RADIUS_KM) {
            continue;
        }

        int slot = find_aircraft_slot(f.icao24);
        bool is_new = (slot < 0);

        if (is_new) {
            slot = s_aircraft_count;
            if (s_aircraft_count >= MAX_AIRCRAFT_OBJECTS) {
                xSemaphoreGive(g_flights_mutex);
                return;
            }
            s_aircraft_count++;

            lv_obj_t *scr = lv_scr_act();

            create_aircraft_icon(s_aircraft[slot], scr, f.kind, dot_color);

            s_aircraft[slot].label = lv_label_create(scr);
            lv_obj_set_style_text_color(s_aircraft[slot].label, label_color, LV_PART_MAIN);
            lv_obj_set_style_text_font(s_aircraft[slot].label, &lv_font_montserrat_12, LV_PART_MAIN);
            s_aircraft[slot].label_text[0] = '\0';

            strncpy(s_aircraft[slot].icao24, f.icao24, sizeof(s_aircraft[slot].icao24) - 1);
            s_aircraft[slot].icao24[sizeof(s_aircraft[slot].icao24) - 1] = '\0';
            s_aircraft[slot].first_seen_time_ms = now_ms;
        }

        copy_trimmed(s_aircraft[slot].callsign, sizeof(s_aircraft[slot].callsign), f.callsign);
        copy_trimmed(s_aircraft[slot].type_code, sizeof(s_aircraft[slot].type_code), f.type_code);
        copy_trimmed(s_aircraft[slot].kind, sizeof(s_aircraft[slot].kind), f.kind);

        if (!obj_valid(s_aircraft[slot].icon_root)) {
            s_aircraft[slot].icon_root = nullptr;
            s_aircraft[slot].icon_img = nullptr;
        }
        if (!obj_valid(s_aircraft[slot].icon_img)) {
            s_aircraft[slot].icon_img = nullptr;
        }
        if (!obj_valid(s_aircraft[slot].label)) {
            s_aircraft[slot].label = nullptr;
            s_aircraft[slot].label_text[0] = '\0';
        }

        const char *icon_kind = normalized_icon_kind(s_aircraft[slot].kind);
        if (s_aircraft[slot].icon_root == nullptr || strncmp(s_aircraft[slot].icon_kind, icon_kind, sizeof(s_aircraft[slot].icon_kind)) != 0) {
            create_aircraft_icon(s_aircraft[slot], lv_scr_act(), s_aircraft[slot].kind, dot_color);
        }
        if (s_aircraft[slot].label == nullptr) {
            s_aircraft[slot].label = lv_label_create(lv_scr_act());
            lv_obj_set_style_text_color(s_aircraft[slot].label, label_color, LV_PART_MAIN);
            lv_obj_set_style_text_font(s_aircraft[slot].label, &lv_font_montserrat_12, LV_PART_MAIN);
            s_aircraft[slot].label_text[0] = '\0';
        }
        copy_trimmed(s_aircraft[slot].departure, sizeof(s_aircraft[slot].departure), f.departure);
        copy_trimmed(s_aircraft[slot].arrival, sizeof(s_aircraft[slot].arrival), f.arrival);
        copy_trimmed(s_aircraft[slot].callsign_iata, sizeof(s_aircraft[slot].callsign_iata), f.callsign_iata);
        if (s_aircraft[slot].departure[0] == '\0') {
            copy_trimmed(s_aircraft[slot].departure, sizeof(s_aircraft[slot].departure), "----");
        }
        if (s_aircraft[slot].arrival[0] == '\0') {
            copy_trimmed(s_aircraft[slot].arrival, sizeof(s_aircraft[slot].arrival), "----");
        }
        s_aircraft[slot].altitude_m = f.altitude_m;
        s_aircraft[slot].speed_mps = f.speed_mps;
        s_aircraft[slot].heading_deg = f.heading_deg;

        lv_point_t pos = flight_to_screen(f);
        s_aircraft[slot].screen_x = (int16_t)pos.x;
        s_aircraft[slot].screen_y = (int16_t)pos.y;

        if (obj_valid(s_aircraft[slot].icon_root)) {
            lv_obj_set_pos(s_aircraft[slot].icon_root, pos.x - (AIRCRAFT_ICON_SIZE / 2), pos.y - (AIRCRAFT_ICON_SIZE / 2));
            if (obj_valid(s_aircraft[slot].icon_img)) {
                lv_obj_set_pos(s_aircraft[slot].icon_img, 0, 0);
            }
        }

        const uint32_t age_ms = now_ms - s_aircraft[slot].first_seen_time_ms;
        const bool flash = (age_ms < 2000) && ((age_ms / 200) % 2 == 0);
        const uint8_t opa = flash ? LV_OPA_COVER : LV_OPA_70;
        if (obj_valid(s_aircraft[slot].icon_root)) {
            lv_obj_set_style_opa(s_aircraft[slot].icon_root, opa, LV_PART_MAIN);
        }

        char label_buf[16];
        const char *callsign = f.callsign;
        if (callsign == nullptr || callsign[0] == '\0') {
            callsign = f.icao24;
        }
        snprintf(label_buf, sizeof(label_buf), "%.6s", callsign);

        set_label_if_changed(s_aircraft[slot].label, s_aircraft[slot].label_text, sizeof(s_aircraft[slot].label_text), label_buf);
        if (obj_valid(s_aircraft[slot].label) && obj_valid(s_aircraft[slot].icon_root)) {
            lv_obj_align_to(s_aircraft[slot].label, s_aircraft[slot].icon_root, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
        }

        s_aircraft[slot].last_update_time_ms = now_ms;
        visible_count++;
    }

    int slot_idx = 0;
    while (slot_idx < s_aircraft_count) {
        const uint32_t age_ms = now_ms - s_aircraft[slot_idx].last_update_time_ms;
        if (age_ms > 60000) {
            delete_icon(s_aircraft[slot_idx]);
            if (obj_valid(s_aircraft[slot_idx].label)) {
                lv_obj_del(s_aircraft[slot_idx].label);
            }
            s_aircraft[slot_idx].label = nullptr;
            s_aircraft[slot_idx].label_text[0] = '\0';

            if (slot_idx == s_selected_slot) {
                detail_panel_hide();
            }

            if (slot_idx < s_aircraft_count - 1) {
                memmove(&s_aircraft[slot_idx], &s_aircraft[slot_idx + 1],
                        (s_aircraft_count - slot_idx - 1) * sizeof(s_aircraft[0]));
                if (s_selected_slot > slot_idx) {
                    s_selected_slot--;
                }
            }
            s_aircraft_count--;
        } else {
            slot_idx++;
        }
    }

    if (s_selected_slot >= 0 && !s_selected_waiting_route && (now_ms - s_selected_at_ms) > DETAIL_AUTO_CLOSE_MS) {
        detail_panel_hide();
    } else if (s_selected_slot >= 0) {
        // Refresh text in case dep/arr arrived after the panel opened
        const AircraftObject &a = s_aircraft[s_selected_slot];
        char line1[48] = {0};
        const bool has_route = (a.departure[0] != '\0' && strncmp(a.departure, "----", 4) != 0
                             && a.arrival[0] != '\0' && strncmp(a.arrival, "----", 4) != 0);
        if (has_route) {
            snprintf(line1, sizeof(line1), "DEP %s  ARR %s", a.departure, a.arrival);
            if (s_selected_waiting_route) {
                // Start the close timeout only once dep/arr is actually available.
                s_selected_waiting_route = false;
                s_selected_route_na = false;
                s_selected_at_ms = now_ms;
            }
        } else if (s_selected_waiting_route) {
            const uint32_t wait_ms = now_ms - s_selected_at_ms;
            if (wait_ms >= DETAIL_ROUTE_WAIT_TIMEOUT_MS) {
                s_selected_waiting_route = false;
                s_selected_route_na = true;
                s_selected_at_ms = now_ms;
                snprintf(line1, sizeof(line1), "N/A");
            } else {
                snprintf(line1, sizeof(line1), "Retrieving...");
            }
        } else if (s_selected_route_na) {
            snprintf(line1, sizeof(line1), "N/A");
        }
        set_label_if_changed(s_detail_line1, s_detail_line1_cache, sizeof(s_detail_line1_cache), line1);
        set_label_if_changed(s_detail_iata,  s_detail_iata_cache,  sizeof(s_detail_iata_cache),  a.callsign_iata[0] ? a.callsign_iata : "");

        char line2[64];
        snprintf(line2, sizeof(line2), "ALT %.0fm  SPD %.0fm/s", a.altitude_m, a.speed_mps);
        set_label_if_changed(s_detail_line2, s_detail_line2_cache, sizeof(s_detail_line2_cache), line2);
    }

    xSemaphoreGive(g_flights_mutex);
}
