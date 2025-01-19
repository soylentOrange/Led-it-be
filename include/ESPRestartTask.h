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
        void restartDelayed(unsigned long delayBeforeCleanup, unsigned long delayBeforeRestart);  
    private:
        Task* _cleanupBeforeRestartTask;
        Task* _restartTask;
        void _cleanupCallback();
        void _restartCallback();
        unsigned long _delayBeforeRestart;
        Scheduler* _scheduler;
    };
} // namespace Soylent
