#pragma once

#include <cstdint>
#include "model/flight.h"

/**
 * Initialize aircraft layer (LVGL objects).
 * Must be called after ui_init(), from display task only.
 */
void aircraft_layer_init();

/**
 * Update and render aircraft dots.
 * Must be called from display task only, once per frame.
 * Reads g_flights[] under mutex protection.
 */
void aircraft_layer_update();

/**
 * Handle a touch tap in screen coordinates.
 * Must be called from display task only.
 */
void aircraft_layer_handle_tap(int16_t x, int16_t y);
