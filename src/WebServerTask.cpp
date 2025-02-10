// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <thingy.h>

#define TAG "WebServer"

// gzipped assets
extern const uint8_t logo_start[] asm("_binary__pio_assets_logo_captive_svg_gz_start");
extern const uint8_t logo_end[] asm("_binary__pio_assets_logo_captive_svg_gz_end");

Soylent::WebServerClass::WebServerClass(AsyncWebServer& webServer)
    : _scheduler(nullptr), _webServer(&webServer) {
  _sr.setWaiting();
}

void Soylent::WebServerClass::begin(Scheduler* scheduler) {
  // Just to be sure that the static webserver is not running anymore before being started (again)
  _webServer->end();

  // Task handling
  _sr.setWaiting();
  _scheduler = scheduler;
  // create and run a task for setting up the (static) webserver
  Task* webServerTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _webServerCallback(); }, _scheduler, false, NULL, NULL, true);
  webServerTask->enable();

  LOGD(TAG, "WebServer is scheduled for start...");
}

void Soylent::WebServerClass::end() {
  LOGD(TAG, "Disabling WebServer-Task...");
  _sr.setWaiting();
  _webServer->end();
  LOGD(TAG, "...done!");
}

// Start the webserver
void Soylent::WebServerClass::_webServerCallback() {
  LOGD(TAG, "Starting WebServer...");

  // serve the logo (for captive portal)
  _webServer->on("/logo", HTTP_GET, [&](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve captive logo...");
              AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", logo_start, logo_end - logo_start);
              response->addHeader("Content-Encoding", "gzip");
              response->addHeader("Cache-Control", "public, max-age=900");
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() == Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // clear persisted wifi config
  _webServer->on("/clearwifi", HTTP_GET, [&](AsyncWebServerRequest* request) {
    LOGW(TAG, "Clearing WiFi configuration...");
    ESPNetwork.clearConfiguration();
    LOGW(TAG, TAG, "Restarting!");
    ESPRestart.restartDelayed(500, 500); // start task for delayed restart
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "WiFi credentials are gone! Restarting now...");
    request->send(response);
  });

  // do restart
  _webServer->on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
    LOGW(TAG, "Restarting!");
    ESPRestart.restartDelayed(500, 500); // start task for delayed restart
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Restarting now...");
    request->send(response);
  });

  // restart from safeboot-partition
  _webServer->on("/safeboot", HTTP_GET, [&](AsyncWebServerRequest* request) {
    LOGW(TAG, "Restart from safeboot...");
    const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot");
    if (partition) {
      esp_ota_set_boot_partition(partition);
      ESPRestart.restartDelayed(500, 500); // start task for delayed restart
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Restarting into SafeBoot now...");
      request->send(response);
    } else {
      LOGW(TAG, "SafeBoot partition not found");
      ESPRestart.restartDelayed(500, 500); // start task for delayed restart
      AsyncWebServerResponse* response = request->beginResponse(502, "text/plain", "SafeBoot partition not found!");
      request->send(response);
    }
  });

  // Set 404-handler only when the captive portal is not shown
  if (EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED) {
    LOGD(TAG, "Register 404 handler in WebServer");
    _webServer->onNotFound([](AsyncWebServerRequest* request) {
      LOGW(TAG, "Send 404 on request for %s", request->url().c_str());
      request->send(404);
    });
  } else {
    LOGD(TAG, "Skip registering 404 handler in WebServer");
  }

  _webServer->begin();

  LOGD(TAG, "...done!");
  _sr.signalComplete();
}

StatusRequest* Soylent::WebServerClass::getStatusRequest() {
  return &_sr;
}
