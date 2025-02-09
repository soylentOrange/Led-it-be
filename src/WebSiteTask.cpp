// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024-2025 Robert Wendlandt
 */
#include <thingy.h>
#define TAG "WebSite"

// gzipped assets
extern const uint8_t logo_thingy_start[] asm("_binary__pio_assets_logo_thingy_svg_gz_start");
extern const uint8_t logo_thingy_end[] asm("_binary__pio_assets_logo_thingy_svg_gz_end");
extern const uint8_t touchicon_start[] asm("_binary__pio_assets_apple_touch_icon_png_gz_start");
extern const uint8_t touchicon_end[] asm("_binary__pio_assets_apple_touch_icon_png_gz_end");
extern const uint8_t favicon_svg_start[] asm("_binary__pio_assets_favicon_svg_gz_start");
extern const uint8_t favicon_svg_end[] asm("_binary__pio_assets_favicon_svg_gz_end");
extern const uint8_t favicon_96_png_start[] asm("_binary__pio_assets_favicon_96x96_png_gz_start");
extern const uint8_t favicon_96_png_end[] asm("_binary__pio_assets_favicon_96x96_png_gz_end");
extern const uint8_t favicon_ico_start[] asm("_binary__pio_assets_favicon_ico_gz_start");
extern const uint8_t favicon_ico_end[] asm("_binary__pio_assets_favicon_ico_gz_end");
extern const uint8_t thingy_html_start[] asm("_binary__pio_assets_thingy_html_gz_start");
extern const uint8_t thingy_html_end[] asm("_binary__pio_assets_thingy_html_gz_end");

// constants from build process
extern const char* __COMPILED_BUILD_BOARD__;
extern char* __COMPILED_BUILD_TIMESTAMP__;

Soylent::WebSiteClass::WebSiteClass(AsyncWebServer& webServer)
    : _ledStateIdx(0), _setLEDHandler(nullptr), _scheduler(nullptr), _ledStateCount(LED_STATES_PLAIN)
#ifdef RGB_BUILTIN
      , _fsMounted(false), _ledStatesJson(nullptr)
#endif
      , _webServer(&webServer) {
}

void Soylent::WebSiteClass::begin(Scheduler* scheduler) {
  LOGD(TAG, "Enabling WebSite-Task...");

  // Task handling
  _scheduler = scheduler;
  // create and run a task for setting up the website
  Task* webSiteTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _webSiteCallback(); }, _scheduler, false, NULL, NULL, true);
  webSiteTask->enable();
  webSiteTask->waitFor(WebServer.getStatusRequest());
}

void Soylent::WebSiteClass::end() {
  if (_setLEDHandler != nullptr) {
    delete _setLEDHandler;
    _setLEDHandler = nullptr;
  }

#ifdef RGB_BUILTIN
  LittleFS.end();
  if (_ledStatesJson != nullptr) {
    delete _ledStatesJson;
    _ledStatesJson = nullptr;
  }
#endif
}

// Add Handlers to the webserver
void Soylent::WebSiteClass::_webSiteCallback() {
  LOGD(TAG, "Starting WebSite...");

#ifdef RGB_BUILTIN
  // try reading /led_states.json
  LOGD(TAG, "Reading led_states.json...");
  File file = LittleFS.open("/led_states.json", "r");
  if (!file || file.isDirectory()) {
    LOGE(TAG, "An Error has occurred while reading led_states.json!");
  } else {
    std::string fileContent;
    _ledStatesJson = new JsonDocument();
    while (file.available()) {
      char intRead = file.read();
      fileContent += intRead;
    }
    file.close();
    deserializeJson(*_ledStatesJson, fileContent);
    _ledStatesJson->shrinkToFit();
    if (!_ledStatesJson->as<JsonObject>()["led_states"].is<JsonArray>()) {
      LOGE(TAG, "An Error has occurred while parsing led_states.json for led_states!");
    } else {
      _ledStateCount = _ledStatesJson->as<JsonObject>()["led_states"].as<JsonArray>().size();
      LOGI(TAG, "led_states.json seems fine! (%d additional led_states)", _ledStateCount);
      _ledStateCount += LED_STATES_PLAIN;
      _fsMounted = true;
    }
  }
#else
  _ledStateCount = LED_STATES_PLAIN;
#endif

#ifdef RGB_BUILTIN
  // serve from File System
  _webServer->serveStatic("/images/", LittleFS, "/").setFilter([&](__unused AsyncWebServerRequest* request) { return _fsMounted; });

  // serve from File System
  _webServer->serveStatic("/led_states.json", LittleFS, "/led_states.json", "no-store").setFilter([&](__unused AsyncWebServerRequest* request) { return _fsMounted; });
#endif

  // Prepare handler for setting led state
  _setLEDHandler = new AsyncCallbackJsonWebHandler("/led/state");
  _setLEDHandler->setMethod(HTTP_PUT);
#ifdef RGB_BUILTIN
  _setLEDHandler->setFilter([&](__unused AsyncWebServerRequest* request) { return (EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED && _fsMounted); });
#else
  _setLEDHandler->setFilter([&](__unused AsyncWebServerRequest* request) { return (EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED); });
#endif
  _setLEDHandler->onRequest([&](AsyncWebServerRequest* request, JsonVariant& json) {
    LOGD(TAG, "Serve (put) /led/state");
    auto led_state_idx = json.as<JsonObject>()["state_idx"].as<int32_t>();
    LOGD(TAG, "Got state_idx: %d", led_state_idx);
#ifndef LED_BUILTIN
    LOGW(TAG, "LED not available");
    request->send(503, "text/plain", "LED not available");
#else
    if (led_state_idx < 0 || led_state_idx > _ledStateCount - 1) {
      LOGW(TAG, "state_idx out of bounds");
      request->send(418, "text/plain", "state_idx out of bounds");
    } else {
      switch (led_state_idx) {
        case 0:
          // 0 is hardcoded to off
          LOGI(TAG, "Switch LED to off!");
          Led.setLedState(Soylent::LedClass::LedState::OFF);
          _ledStateIdx = led_state_idx;
          break;
        case 1:
          // 1 is hardcoded to on
          LOGI(TAG, "Switch LED to on!");
          Led.setLedState(Soylent::LedClass::LedState::ON);
          _ledStateIdx = led_state_idx;
          break;
        case 2:
          // 2 is hardcoded to Blinking
          LOGI(TAG, "Switch LED to on and off and on and ...!");
          Led.setLedState(Soylent::LedClass::LedState::BLINK);
          _ledStateIdx = led_state_idx;
          break;
        case 3:
  // 3 is hardcoded to Rainbow
  #ifdef RGB_BUILTIN
          LOGI(TAG, "Show a beautiful rainbow...!");
  #else
          LOGI(TAG, "Show a boring rainbow...!");
  #endif
          Led.setLedState(Soylent::LedClass::LedState::RAINBOW);
          _ledStateIdx = led_state_idx;
          break;
        default: {
          // show an something else?
          LOGI(TAG, "I want to show something!");
          // auto img_name = _imagesJson->as<JsonObject>()["images"].as<JsonArray>()[img_idx-1].as<JsonObject>()["src"];
          // Display.showImage(img_name);
          _ledStateIdx = led_state_idx;
        }
      }

      request->send(200, "text/plain", "OK");
    }
#endif
  });

  // Register handler for setting led state
  _webServer->addHandler(_setLEDHandler);

  // serve request for setting led state
  _webServer->on("/led/state", HTTP_GET, [&](AsyncWebServerRequest* request) {
              // LOGD(TAG, "Serve (get) /led/state");
              AsyncResponseStream* response = request->beginResponseStream("application/json");
              JsonDocument doc;
              JsonObject root = doc.to<JsonObject>();

              root["state"] = "idle";
              root["state_idx"] = _ledStateIdx;
              serializeJson(root, *response);
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve the logo (for main page)
  _webServer->on("/thingy_logo", HTTP_GET, [](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve thingy logo...");
              AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", logo_thingy_start, logo_thingy_end - logo_thingy_start);
              response->addHeader("Content-Encoding", "gzip");
              response->addHeader("Cache-Control", "public, max-age=900");
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve the favicon.svg
  _webServer->on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve favicon.svg...");
              AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", favicon_svg_start, favicon_svg_end - favicon_svg_start);
              response->addHeader("Content-Encoding", "gzip");
              response->addHeader("Cache-Control", "public, max-age=900");
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve the apple-touch-icon
  _webServer->on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve apple-touch-icon.png...");
              AsyncWebServerResponse* response = request->beginResponse(200, "image/png", touchicon_start, touchicon_end - touchicon_start);
              response->addHeader("Content-Encoding", "gzip");
              response->addHeader("Cache-Control", "public, max-age=900");
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve the favicon.png
  _webServer->on("/favicon-96x96.png", HTTP_GET, [](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve favicon-96x96.png...");
              AsyncWebServerResponse* response = request->beginResponse(200, "image/png", favicon_96_png_start, favicon_96_png_end - favicon_96_png_start);
              response->addHeader("Content-Encoding", "gzip");
              response->addHeader("Cache-Control", "public, max-age=900");
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve the favicon.png
  _webServer->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve favicon.ico...");
              AsyncWebServerResponse* response = request->beginResponse(200, "image/x-icon", favicon_ico_start, favicon_ico_end - favicon_ico_start);
              response->addHeader("Content-Encoding", "gzip");
              response->addHeader("Cache-Control", "public, max-age=900");
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve boardname info
  _webServer->on("/boardname", HTTP_GET, [](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve boardname");
              AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", __COMPILED_BUILD_BOARD__);
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve boardname info
  _webServer->on("/buildtime", HTTP_GET, [](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve buildtime");
              AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", __COMPILED_BUILD_TIMESTAMP__);
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  // serve our home page here, yet only when the ESP32Connect portal is not shown
  _webServer->on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
              LOGD(TAG, "Serve...");
              AsyncWebServerResponse* response = request->beginResponse(200, "text/html", thingy_html_start, thingy_html_end - thingy_html_start);
              response->addHeader("Content-Encoding", "gzip");
              request->send(response);
            })
    .setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Soylent::ESP32Connect::State::PORTAL_STARTED; });

  LOGD(TAG, "...done!");
}
