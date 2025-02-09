// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
  class ESPConnectClass {
    public:
      ESPConnectClass(Soylent::ESP32Connect& espConnect);
      void begin(Scheduler* scheduler);
      void end();
      void clearConfiguration();

    private:
      Task* _espConnectTask;
      void _espConnectCallback();
      Scheduler* _scheduler;
      Soylent::ESP32Connect* _espConnect;
  };
} // namespace Soylent
