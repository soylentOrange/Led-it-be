// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <thingy.h>
#define TAG "ESPConnect"

Soylent::ESPConnectClass::ESPConnectClass(Soylent::ESP32Connect& espConnect)
    : _espConnectTask(nullptr), _scheduler(nullptr), _espConnect(&espConnect) {
}

void Soylent::ESPConnectClass::begin(Scheduler* scheduler) {
  LOGD(TAG, "Schedule ESPConnect...");
  // stop possibly running espConnect first
  if (_espConnect->getState() != Soylent::ESP32Connect::State::NETWORK_DISABLED)
    _espConnect->end();

  // get some info from espconnect's preferences
  Preferences preferences;
  preferences.begin("espconnect", true);
  std::string ssid;
  if (preferences.isKey("ssid"))
    ssid = preferences.getString("ssid").c_str();
  bool ap = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
  preferences.end();

  if (ssid.empty() || ap) {
    LOGI(TAG, "Trying to start captive portal in the background...");
  } else {
    LOGI(TAG, "Trying to connect to saved WiFi (%s) in the background...", ssid.c_str());
  }

  // configure and begin espConnect
  _espConnect->setAutoRestart(true);
  _espConnect->setBlocking(false);
  _espConnect->setCaptivePortalTimeout(ESPCONNECT_TIMEOUT_CAPTIVE_PORTAL);
  _espConnect->setConnectTimeout(ESPCONNECT_TIMEOUT_CONNECT);
  _espConnect->begin(APP_NAME, CAPTIVE_PORTAL_SSID, CAPTIVE_PORTAL_PASSWORD);

  // Task handling
  _scheduler = scheduler;
  _espConnectTask = new Task(TASK_IMMEDIATE, TASK_FOREVER, [&] { _espConnectCallback(); }, _scheduler, false, NULL, NULL, true);
  _espConnectTask->enable();

  LOGD(TAG, "ESPConnect is scheduled for start...");
}

void Soylent::ESPConnectClass::end() {
  LOGD(TAG, "Stopping ESPConnect...");
  _espConnectTask->disable();
  _espConnect->end();
  LOGD(TAG, "...done!");
}

void Soylent::ESPConnectClass::clearConfiguration() {
  _espConnect->clearConfiguration();
}

// Loop espConnect
void Soylent::ESPConnectClass::_espConnectCallback() {
  _espConnect->loop();

  if (_espConnectTask->isFirstIteration()) {
    LOGD(TAG, "ESPConnect started and looping now!");
  }
}
