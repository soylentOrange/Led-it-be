// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <thingy.h>
#define TAG "EventHandler"

Soylent::EventHandlerClass::EventHandlerClass(AsyncWebServer& webServer, Mycila::ESPConnect& espConnect)
    : _state(Mycila::ESPConnect::State::NETWORK_DISABLED)
    , _scheduler(nullptr)
    , _webServer(&webServer)
    , _espConnect(&espConnect) {
}

void Soylent::EventHandlerClass::begin(Scheduler* scheduler) {
    _state = _espConnect->getState();

    // Task handling
    _scheduler = scheduler;

    // Register Callback to espConnect
    _espConnect->listen([&](__unused Mycila::ESPConnect::State previous, 
        Mycila::ESPConnect::State state) { _stateCallback(state); });
}

void Soylent::EventHandlerClass::end() {
    LOGD(TAG, "Disabling EventHandler...");
    _espConnect->listen([](__unused Mycila::ESPConnect::State previous,
        __unused Mycila::ESPConnect::State state) { std::function<void(void)> {}; });
    _state = Mycila::ESPConnect::State::NETWORK_DISABLED;
}

Mycila::ESPConnect::State Soylent::EventHandlerClass::getState() {
    return _state;
}

// Handle events from ESPConnect
void Soylent::EventHandlerClass::_stateCallback(Mycila::ESPConnect::State state) {
    _state = state;

    switch (state) {
        case Mycila::ESPConnect::State::NETWORK_CONNECTED:
            LOGI(TAG, "--> Connected to network...");
            yield();
            LOGI(TAG, "IPAddress: %s", _espConnect->getIPAddress().toString().c_str());
            WebServer.begin(_scheduler);
            yield();
            WebSite.begin(_scheduler);
            break;

        case Mycila::ESPConnect::State::AP_STARTED:
            LOGI(TAG, "--> Created AP...");
            yield();
            LOGI(TAG, "SSID: %s", _espConnect->getAccessPointSSID().c_str());
            LOGI(TAG, "IPAddress: %s", _espConnect->getIPAddress().toString().c_str());
            WebServer.begin(_scheduler);
            yield();
            WebSite.begin(_scheduler);
            break;

        case Mycila::ESPConnect::State::PORTAL_STARTED:
            LOGI(TAG, "--> Started Captive Portal...");
            yield();
            LOGI(TAG, "SSID: %s", _espConnect->getAccessPointSSID().c_str());
            LOGI(TAG, "IPAddress: %s", _espConnect->getIPAddress().toString().c_str());
            WebServer.begin(_scheduler);
            break;

        case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
            LOGI(TAG, "--> Disconnected from network...");
            WebSite.end();
            WebServer.end();
            break;

        case Mycila::ESPConnect::State::PORTAL_COMPLETE: {
            LOGI(TAG, "--> Captive Portal has ended, auto-save the configuration...");
            auto config = _espConnect->getConfig();
            LOGD(TAG, "ap: %d", config.apMode);
            LOGD(TAG, "wifiSSID: %s", config.wifiSSID.c_str());
            LOGD(TAG, "wifiPassword: %s", config.wifiPassword.c_str());
            break;
        }

        default:
            break;
    } /* switch (_state) */
}
