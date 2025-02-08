// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
    class WebSiteClass {
    public:
        WebSiteClass(AsyncWebServer& webServer);
        void begin(Scheduler* scheduler);
        void end();

    private:
        void _webSiteCallback();
        AsyncCallbackJsonWebHandler* _setLEDHandler;
        Scheduler* _scheduler;
        AsyncWebServer* _webServer;
        int32_t _ledStateCount;
        #ifdef RGB_BUILTIN
            bool _fsMounted = false;     
            JsonDocument* _ledStatesJson;
        #endif   
        int32_t _ledStateIdx;
    };
} // namespace Soylent
