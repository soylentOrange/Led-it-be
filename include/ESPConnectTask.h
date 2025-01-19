// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
    class ESPConnectClass {
    public:
        ESPConnectClass(Mycila::ESPConnect& espConnect);
        void begin(Scheduler* scheduler);
        void end();
        void clearConfiguration();

    private:
        Task* _espConnectTask;
        void _espConnectCallback();
        Scheduler* _scheduler;
        Mycila::ESPConnect* _espConnect;
    };
} // namespace Soylent
