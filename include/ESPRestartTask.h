// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
  class ESPRestartClass {
    public:
      ESPRestartClass();
      void begin(Scheduler* scheduler);
      void restart();
      void restartDelayed(uint32_t delayBeforeCleanup, uint32_t delayBeforeRestart);

    private:
      Task* _cleanupBeforeRestartTask;
      Task* _restartTask;
      void _cleanupCallback();
      void _restartCallback();
      uint32_t _delayBeforeRestart;
      Scheduler* _scheduler;
  };
} // namespace Soylent
