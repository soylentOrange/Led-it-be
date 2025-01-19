// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou, 2024 Robert Wendlandt
 */
#include <safeboot.h>

// The WebServer, obviously...
AsyncWebServer webServer(HTTP_PORT);
SafeBootOTAConnect otaConnect(webServer);

void setup() {

    // Start Serial or USB-CDC
    #if !ARDUINO_USB_CDC_ON_BOOT
        Serial.begin(MONITOR_SPEED);
        // Only wait for serial interface to be set up when not using USB-CDC
        while (!Serial)
            yield();
    #else
        // USB-CDC doesn't need a baud rate
        Serial.begin();
        Serial.enableReboot(true);
        Serial.setDebugOutput(true);
    #endif

    // get config from preferences
    Preferences preferences;
    preferences.begin("safeboot", true);
    String app_name = preferences.getString("app_name", "Safeboot");
    String ap_ssid = preferences.getString("ssid", "Safeboot");
    String ap_password = preferences.getString("pass", "");
    preferences.end();

    // Start SafeBootOTAConnect
    log_d("Blockingly trying to connect to saved WiFi or start portal...");
    otaConnect.begin(app_name.c_str(), ap_ssid.c_str(), ap_password.c_str());

    // Now we should be connected to either the known Wifi or our own Soft-AP
    log_d("Setup finished!");
}

void loop() {
    otaConnect.loop();
}
