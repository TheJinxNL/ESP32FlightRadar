#include "display.h"
#include "../config.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <esp_timer.h>

// ============================================================
// LovyanGFX — GC9A01 on SPI2
// ============================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;

public:
    LGFX() {
        // SPI bus
        {
            auto cfg      = _bus.config();
            cfg.spi_host  = SPI2_HOST;
            cfg.spi_mode  = 0;
            cfg.freq_write = 80000000;
            cfg.freq_read  = 20000000;
            cfg.pin_sclk  = PIN_LCD_SCLK;
            cfg.pin_mosi  = PIN_LCD_MOSI;
            cfg.pin_miso  = -1;
            cfg.pin_dc    = PIN_LCD_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        // Panel
        {
            auto cfg         = _panel.config();
            cfg.pin_cs       = PIN_LCD_CS;
            cfg.pin_rst      = PIN_LCD_RST;
            cfg.pin_busy     = -1;
            cfg.panel_width  = LCD_WIDTH;
            cfg.panel_height = LCD_HEIGHT;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable     = false;
            cfg.invert       = true;  // GC9A01 panels on this board need inverted color mode
            cfg.rgb_order    = false;
            cfg.dlen_16bit   = false;
            cfg.bus_shared   = false;
            _panel.config(cfg);
        }

        // Backlight (PWM)
        {
            auto cfg      = _light.config();
            cfg.pin_bl    = PIN_LCD_BL;
            cfg.invert    = false;
            cfg.freq      = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }

        setPanel(&_panel);
    }
};

static LGFX lgfx_dev;

// ============================================================
// LVGL draw buffers (double-buffered)
// ============================================================
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         buf_a[LCD_WIDTH * LVGL_BUF_LINES];
static lv_color_t         buf_b[LCD_WIDTH * LVGL_BUF_LINES];

// ============================================================
// LVGL flush callback — hand pixels to LovyanGFX
// ============================================================
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

    lgfx_dev.startWrite();
    lgfx_dev.setAddrWindow(area->x1, area->y1, w, h);
    lgfx_dev.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    lgfx_dev.endWrite();

    lv_disp_flush_ready(drv);
}

// ============================================================
// LVGL tick source — 1 ms hardware timer
// ============================================================
static void lvgl_tick_cb(void * /*arg*/) {
    lv_tick_inc(1);
}

// ============================================================
// Public init
// ============================================================
void display_init() {
    lgfx_dev.init();
    lgfx_dev.setBrightness(200);
    lgfx_dev.setColorDepth(16);
    lgfx_dev.fillScreen(TFT_BLACK);

    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf_a, buf_b, LCD_WIDTH * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res    = LCD_WIDTH;
    disp_drv.ver_res    = LCD_HEIGHT;
    disp_drv.flush_cb   = lvgl_flush_cb;
    disp_drv.draw_buf   = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 1 ms periodic timer for LVGL tick
    const esp_timer_create_args_t timer_args = {
        .callback        = lvgl_tick_cb,
        .arg             = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "lvgl_tick",
        .skip_unhandled_events = true
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 1000); // 1 ms
}
