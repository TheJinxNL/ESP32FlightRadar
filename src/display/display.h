#pragma once

#include <lvgl.h>

/**
 * Initialise LovyanGFX (GC9A01) and wire it up to LVGL.
 * Must be called once before any lv_* calls.
 * After this call, lv_timer_handler() drives rendering.
 */
void display_init();
