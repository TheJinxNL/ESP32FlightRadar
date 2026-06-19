#pragma once

// Download and decode the Google Static Map once for this boot cycle.
// Safe to call multiple times; only the first successful call stores a map.
bool map_background_fetch_once();

// Returns a short human-readable status for the latest map request attempt.
const char *map_background_last_status();

// Install decoded map as the bottom-most LVGL object when ready.
// Must be called from the display/LVGL task.
void map_background_try_install();
