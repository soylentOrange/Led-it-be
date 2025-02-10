// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <Esp.h>
#include <thingy.h>
#define TAG "ESPRestart"

Soylent::ESPRestartClass::ESPRestartClass()
    : _cleanupBeforeRestartTask(nullptr), _restartTask(nullptr), _delayBeforeRestart(0), _scheduler(nullptr) {
}

void Soylent::ESPRestartClass::begin(Scheduler* scheduler) {
  // Create Tasks and add them to the scheduler
  _scheduler = scheduler;
  _cleanupBeforeRestartTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _cleanupCallback(); }, _scheduler, false, NULL, NULL, true);
  _restartTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _restartCallback(); }, _scheduler, false, NULL, NULL, true);
}

void Soylent::ESPRestartClass::restart() {
  _delayBeforeRestart = 0;
  _cleanupBeforeRestartTask->enable();
}

void Soylent::ESPRestartClass::restartDelayed(uint32_t delayBeforeCleanup = 500, uint32_t delayBeforeRestart = 500) {
  _delayBeforeRestart = delayBeforeRestart;
  _cleanupBeforeRestartTask->enableDelayed(delayBeforeCleanup);
}

void Soylent::ESPRestartClass::_cleanupCallback() {
  // Do some cleanup...
  WebSite.end();
  WebServer.end();
  ESPNetwork.end();

  // ...and finally, the Restart-Task can be enabled subsequently
  _restartTask->enableDelayed(_delayBeforeRestart);
}

// Just iniate the restart
void Soylent::ESPRestartClass::_restartCallback() {
  esp_restart();
}
