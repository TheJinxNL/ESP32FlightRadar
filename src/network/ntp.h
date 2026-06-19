#pragma once

void ntp_begin();
bool ntp_sync_blocking(unsigned long timeout_ms);
bool ntp_is_synced();
