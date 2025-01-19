// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <MycilaESPConnect.h>

namespace Soylent {
    class EventHandlerClass {
    public:
        EventHandlerClass(AsyncWebServer& webServer, Mycila::ESPConnect& espConnect);
        void begin(Scheduler* scheduler);
        void end();
        Mycila::ESPConnect::State getState();

    private:
        void _stateCallback(Mycila::ESPConnect::State state);
        Mycila::ESPConnect::State _state;
        Scheduler* _scheduler;
        AsyncWebServer* _webServer;
        Mycila::ESPConnect* _espConnect;
    };
} // namespace Soylent
