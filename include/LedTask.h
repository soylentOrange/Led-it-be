// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
    class LedClass {
    public:
        enum class LedState {
            NONE = -1,
            OFF = 0,
            ON = 1,
            BLINK = 2
        };

        LedClass();
        LedClass(uint8_t LED_Pin, bool is_RGB);
        void begin(Scheduler* scheduler);
        void end();
        void setLedState(LedState ledState);
        LedState getLedState();
        bool isInitialized();
        bool isBusy();
        bool isBlinking();

    private:
        // struct for passing parameters to async functions
        struct async_params
        {
            StatusRequest* srBusy;
            StatusRequest* srBlinking;
            LedState ledState;
            uint8_t ledPin;
            bool rgbLed;
            uint32_t blinkIntervall;
        };

        void _initializeLedCallback();
        void _setLedCallback();
        static void _async_setLedTask(void* pvParameters);
        StatusRequest _srInitialized;
        StatusRequest _srBusy;
        StatusRequest _srBlinking;
        Scheduler* _scheduler;
        LedState _ledState;
        uint8_t _ledPin;
        bool _rgbLed;
        uint32_t _blinkIntervall;
        async_params* _async_params;
        TaskHandle_t _async_blink_task_handle;
    };
} // namespace Soylent
