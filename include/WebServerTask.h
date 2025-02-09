// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
  class WebServerClass {
    public:
      explicit WebServerClass(AsyncWebServer& webServer);
      void begin(Scheduler* scheduler);
      void end();
      StatusRequest* getStatusRequest();

    private:
      void _webServerCallback();
      StatusRequest _sr;
      Scheduler* _scheduler;
      AsyncWebServer* _webServer;
  };
} // namespace Soylent
