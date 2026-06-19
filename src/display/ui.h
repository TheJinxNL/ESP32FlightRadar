#pragma once

/**
 * Initialise the LVGL screen for M1.
 * Creates a full-screen black background with a centred "RADAR INIT" label.
 * Call after display_init().
 */
void ui_init();

/**
 * Thread-safe status update API for non-display tasks.
 * Text is applied by the display task on the next animation tick.
 */
void ui_set_network_status(const char *status_text);

/**
 * Thread-safe toggle for captive-portal instructions.
 * When active, an overlay panel explains how to join the setup AP.
 */
void ui_set_wifi_portal_active(bool active);
