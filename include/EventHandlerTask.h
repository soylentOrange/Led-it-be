// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <ESP32Connect.h>
#include <TaskSchedulerDeclarations.h>

namespace Soylent {
  class EventHandlerClass {
    public:
      explicit EventHandlerClass(Soylent::ESPNetworkClass& espNetwork);
      void begin(Scheduler* scheduler);
      void end();
      Soylent::ESPConnect::State getState();

    private:
      void _stateCallback(Soylent::ESPConnect::State state);
      Soylent::ESPConnect::State _state;
      Scheduler* _scheduler;
      Soylent::ESPNetworkClass* _espNetwork;
  };
} // namespace Soylent
