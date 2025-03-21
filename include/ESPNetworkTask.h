// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
  class ESPNetworkClass {
    public:
      explicit ESPNetworkClass(AsyncWebServer& webServer);
      void begin(Scheduler* scheduler);
      void end();
      void clearConfiguration();
      Soylent::ESPConnect* getESPConnect();

    private:
      Task* _espConnectTask;
      void _espConnectCallback();
      Scheduler* _scheduler;
      AsyncWebServer* _webServer;
      Soylent::ESPConnect _espConnect;
  };
} // namespace Soylent
