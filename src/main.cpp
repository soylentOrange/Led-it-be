// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#include <SystemInfo.h>
#include <TaskScheduler.h>
#include <thingy.h>

// Create the WebServer, ESPConnect, Task-Scheduler,... here
AsyncWebServer webServer(HTTP_PORT);
// TODO(@soylentorange): move espConnect to ESPConnectClass
Soylent::ESP32Connect espConnect(webServer);
Scheduler scheduler;
Soylent::ESPRestartClass ESPRestart;
Soylent::ESPConnectClass ESPConnect(espConnect);
Soylent::EventHandlerClass EventHandler(webServer, espConnect);
Soylent::WebServerClass WebServer(webServer);
Soylent::WebSiteClass WebSite(webServer);
Soylent::LedClass Led;

// Spinlock for critical sections
portMUX_TYPE cs_spinlock = portMUX_INITIALIZER_UNLOCKED;

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

  // Note: Enabling Debug via USB-CDC is handled via framework
#endif

  // Get reason for restart
  LOGI(APP_NAME, "Reset reason: %s", SystemInfo.getResetReasonString().c_str());

  // Open preferences and check/assign safeboot content
  // Will be overwritten, whenever the __COMPILED_BUILD_TIMESTAMP__ changes
  extern const char* __COMPILED_BUILD_BOARD__;
  extern char* __COMPILED_BUILD_TIMESTAMP__;
  extern const uint8_t logo_safeboot_start[] asm("_binary__pio_assets_logo_safeboot_svg_gz_start");
  extern const uint8_t logo_safeboot_end[] asm("_binary__pio_assets_logo_safeboot_svg_gz_end");
  Preferences preferences;
  preferences.begin("safeboot", false);
  // TODO(soylentOrange): update logic to not always rewrite...
  if (preferences.getString("build") != static_cast<String>(__COMPILED_BUILD_TIMESTAMP__)) {
    preferences.putString("build", __COMPILED_BUILD_TIMESTAMP__);
    preferences.putString("app_name", APP_NAME);
    preferences.putString("ssid", CAPTIVE_PORTAL_SSID);
    preferences.putString("pass", CAPTIVE_PORTAL_PASSWORD);
    preferences.putString("board", __COMPILED_BUILD_BOARD__);
    preferences.putBytes("logo", logo_safeboot_start, logo_safeboot_end - logo_safeboot_start);
    preferences.putULong("logo_len", logo_safeboot_end - logo_safeboot_start);
  }
  preferences.end();

  // Initialize the Scheduler
  scheduler.init();

// Mount the FS, yet only required when a RGB-LED is available
#ifdef RGB_BUILTIN
  if (!LittleFS.begin(false)) {
    LOGE(APP_NAME, "An Error has occurred while mounting LittleFS!");
  }
#endif

  // Add LED-Task to Scheduler
  Led.begin(&scheduler);

  // Add Restart-Task to Scheduler
  ESPRestart.begin(&scheduler);

  // Add ESPConnect-Task to Scheduler
  ESPConnect.begin(&scheduler);

  // Add EventHandler to Scheduler
  // Will also spawn the WebServer and WebSite (when ESPConnect says so...)
  EventHandler.begin(&scheduler);
}

void loop() {
  scheduler.execute();
}
