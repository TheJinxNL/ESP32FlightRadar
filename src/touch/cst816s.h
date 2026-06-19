#pragma once

#include <stdint.h>

/**
 * Initialise CST816S touch controller over I2C.
 * Returns false when the controller is not reachable.
 */
bool cst816s_init();

/**
 * Poll one tap event.
 * Returns true exactly once per touch-down (debounced).
 */
bool cst816s_poll_tap(int16_t *x, int16_t *y);
