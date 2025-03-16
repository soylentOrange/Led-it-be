// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <thingy.h>
#define TAG "EventHandler"

Soylent::EventHandlerClass::EventHandlerClass(Soylent::ESPNetworkClass& espNetwork)
    : _state(Soylent::ESPConnect::State::NETWORK_DISABLED), _scheduler(nullptr), _espNetwork(&espNetwork) {
}

void Soylent::EventHandlerClass::begin(Scheduler* scheduler) {
  _state = _espNetwork->getESPConnect()->getState();

  // Task handling
  _scheduler = scheduler;

  // Register Callback to espConnect
  _espNetwork->getESPConnect()->listen([&](__unused Soylent::ESPConnect::State previous, Soylent::ESPConnect::State state) {
    _stateCallback(state);
  });
}

void Soylent::EventHandlerClass::end() {
  LOGD(TAG, "Disabling EventHandler...");
  _espNetwork->getESPConnect()->listen(nullptr);
  _state = Soylent::ESPConnect::State::NETWORK_DISABLED;
}

Soylent::ESPConnect::State Soylent::EventHandlerClass::getState() {
  return _state;
}

// Handle events from ESPConnect
void Soylent::EventHandlerClass::_stateCallback(Soylent::ESPConnect::State state) {
  _state = state;

  switch (state) {
    case Soylent::ESPConnect::State::NETWORK_CONNECTED:
      LOGI(TAG, "--> Connected to network...");
      yield();
      LOGI(TAG, "IPAddress: %s", _espNetwork->getESPConnect()->getIPAddress().toString().c_str());
      WebServer.begin(_scheduler);
      yield();
      WebSite.begin(_scheduler);
      break;

    case Soylent::ESPConnect::State::AP_STARTED:
      LOGI(TAG, "--> Created AP...");
      yield();
      LOGI(TAG, "SSID: %s", _espNetwork->getESPConnect()->getAccessPointSSID().c_str());
      LOGI(TAG, "IPAddress: %s", _espNetwork->getESPConnect()->getIPAddress().toString().c_str());
      WebServer.begin(_scheduler);
      yield();
      WebSite.begin(_scheduler);
      break;

    case Soylent::ESPConnect::State::PORTAL_STARTED:
      LOGI(TAG, "--> Started Captive Portal...");
      yield();
      LOGI(TAG, "SSID: %s", _espNetwork->getESPConnect()->getAccessPointSSID().c_str());
      LOGI(TAG, "IPAddress: %s", _espNetwork->getESPConnect()->getIPAddress().toString().c_str());
      WebServer.begin(_scheduler);
      break;

    case Soylent::ESPConnect::State::NETWORK_DISCONNECTED:
      LOGI(TAG, "--> Disconnected from network...");
      WebSite.end();
      WebServer.end();
      break;

    case Soylent::ESPConnect::State::PORTAL_COMPLETE: {
      LOGI(TAG, "--> Captive Portal has ended, auto-save the configuration...");
      auto config = _espNetwork->getESPConnect()->getConfig();
      LOGD(TAG, "ap: %d", config.apMode);
      LOGD(TAG, "wifiSSID: %s", config.wifiSSID.c_str());
      LOGD(TAG, "wifiPassword: %s", config.wifiPassword.c_str());
      break;
    }

    default:
      break;
  } /* switch (_state) */
}
