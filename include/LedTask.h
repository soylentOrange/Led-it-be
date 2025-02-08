// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024-2025 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

// plain simple led-states (off/on/blink) more might be added from led_states.json
#define LED_STATES_PLAIN 3
#define TERMINATE_YOURSELF 1

namespace Soylent {
    class LedClass {
    public:
        enum class LedState {
            NONE = -1,
            OFF = 0,
            ON = 1,
            BLINK = 2,
            // defined here, but is only useful for RGB-LEDs
            RAINBOW = 3
        };

        LedClass();
        LedClass(uint8_t LED_Pin, bool is_RGB);
        void begin(Scheduler* scheduler);
        void end();
        void setLedState(LedState ledState);
        LedState getLedState();
        bool isInitialized();
        bool isBusy();
        bool isAnimated();

    private:
        // struct for passing parameters to async LED tasks
        struct LEDTaskParams {
            struct {
                StatusRequest* srBusy;
                StatusRequest* srAnimated;
                LedState ledState;
                uint8_t ledPin;
                bool rgbLed;
                uint32_t timeConstant;
                uint8_t hue;    // still being ignored
            };

            /// Default constructor
            /// @warning Default values are UNITIALIZED!
            constexpr inline __attribute__((always_inline)) LEDTaskParams()
                : srBusy(nullptr)
                , srAnimated(nullptr)
                , ledState(LedState::NONE)
                , ledPin(0)
                , rgbLed(false)
                , timeConstant(0)
                , hue(0) { 
            }

            /// Allow construction from values
            constexpr inline __attribute__((always_inline)) LEDTaskParams(StatusRequest* srBusy, 
                StatusRequest* srAnimated,
                LedState ledState,
                uint8_t ledPin,
                bool rgbLed,
                uint32_t timeConstant,
                uint8_t hue)
                : srBusy(srBusy)
                , srAnimated(srAnimated)
                , ledState(ledState)
                , ledPin(ledPin)
                , rgbLed(rgbLed)
                , timeConstant(timeConstant)
                , hue(hue) {
            }
        };

        void _initializeLedCallback();
        void _setLedCallback();
        static void _async_setLedTask(void* pvParameters);
        static void _adjustLed(CRGB* led, const CRGB& adjustment);
        StatusRequest _srInitialized;
        StatusRequest _srBusy;
        StatusRequest _srAnimated;
        Scheduler* _scheduler;
        LedState _ledState;
        uint8_t _ledPin;
        bool _rgbLed;
        uint32_t _timeConstant;
        TaskHandle_t _async_task_handle;
    };

} // namespace Soylent
