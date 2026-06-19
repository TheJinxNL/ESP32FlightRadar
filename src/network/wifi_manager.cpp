#include "wifi_manager.h"
#include "../config.h"
#include "../display/ui.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <string.h>

static void wifi_portal_started(WiFiManager *wm) {
    (void)wm;
    Serial.println("[wifi] config portal active");
    ui_set_wifi_portal_active(true);
}

bool wifi_connect_blocking() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    // Keep credentials across reboot so WiFiManager can reconnect automatically.
    WiFi.persistent(true);

    WiFiManager wm;
    wm.setDebugOutput(true);
    wm.setConfigPortalBlocking(true);
    wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);
    wm.setConfigPortalTimeout(WIFI_MANAGER_PORTAL_TIMEOUT_S);
    wm.setAPCallback(wifi_portal_started);

    ui_set_wifi_portal_active(false);

    Serial.println("[wifi] preparing connection");

    bool ok = false;
#if WIFI_MANAGER_FORCE_PORTAL_ON_BOOT
    Serial.println("[wifi] forced portal mode on boot");
    if (WIFI_MANAGER_CLEAR_SAVED_ON_BOOT) {
        Serial.println("[wifi] clearing stored credentials");
        wm.resetSettings();
    }

    if (WIFI_MANAGER_AP_PASSWORD[0] != '\0') {
        ok = wm.startConfigPortal(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
    } else {
        ok = wm.startConfigPortal(WIFI_MANAGER_AP_NAME);
    }
#else
    Serial.println("[wifi] trying saved Wi-Fi credentials");
    Serial.println("[wifi] opening setup portal if needed");

    if (WIFI_MANAGER_AP_PASSWORD[0] != '\0') {
        ok = wm.autoConnect(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
    } else {
        ok = wm.autoConnect(WIFI_MANAGER_AP_NAME);
    }
#endif

    if (!ok) {
        Serial.println("[wifi] connect failed or portal timeout");
        ui_set_wifi_portal_active(false);
        return false;
    }

    ui_set_wifi_portal_active(false);
    Serial.printf("[wifi] connected ssid=%s ip=%s\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}
