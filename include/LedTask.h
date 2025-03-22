// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024-2025 Robert Wendlandt
 */
#pragma once

#include <FastLED.h>
#include <TaskSchedulerDeclarations.h>

// plain simple led-states (off/on/blink) more might be added from led_states.json
#define LED_STATES_PLAIN   3
#define TERMINATE_YOURSELF 1
#define DEFAULT_SAT        240
#define DEFAULT_VALUE      255

namespace Soylent {
  class LedClass {
    public:
      enum class LedMode {
        NONE = -1,
        OFF = 0,
        ON = 1,
        BLINK = 2,
        // defined here, but is only useful for RGB-LEDs
        RAINBOW = 3,
        MANUAL = 4
      };

      struct LedState {
          struct {
              uint8_t ledHue;
              bool ledOn;
          };

          /// Default constructor
          /// @warning Default values are UNITIALIZED!
          constexpr inline __attribute__((always_inline)) LedState()
              : ledHue(0), ledOn(false) {
          }

          /// Allow construction from values
          constexpr inline __attribute__((always_inline)) LedState(uint8_t ledHue,
                                                                   bool ledOn = false)
              : ledHue(ledHue), ledOn(ledOn) {
          }
      };

      typedef std::function<void(LedState previous, LedState state)> LEDStateCallback;

      LedClass();
      LedClass(uint8_t LED_Pin, bool is_RGB);
      void begin(Scheduler* scheduler);
      void end();
      // set Animation, On, Off with random color
      void setLedMode(LedMode ledMode);
      void setLed(LedMode ledMode, uint8_t ledHue, bool randomHue = false);
      // get color and on/off-state
      LedState getLedState();
      // get LED-Animation mode
      LedMode getLedMode();
      bool isInitialized();
      bool isBusy();
      bool isAnimated();
      void listen(LEDStateCallback callback) { _externalLedStateCallback = callback; }

    private:
      // struct for passing parameters to async LED tasks
      struct LEDTaskParams {
          struct {
              StatusRequest* srBusy;
              StatusRequest* srAnimated;
              LedMode ledMode;
              uint8_t ledPin;
              bool rgbLed;
              uint32_t timeConstant;
              uint8_t ledHue;
              bool randomHue;
              QueueHandle_t xQueueLedState;
          };

          /// Default constructor
          /// @warning Default values are UNITIALIZED!
          constexpr inline __attribute__((always_inline)) LEDTaskParams()
              : srBusy(nullptr), srAnimated(nullptr), ledMode(LedMode::NONE), ledPin(0), rgbLed(false), timeConstant(0), ledHue(0), randomHue(true), xQueueLedState(nullptr) {
          }

          /// Allow construction from values
          constexpr inline __attribute__((always_inline)) LEDTaskParams(StatusRequest* srBusy,
                                                                        StatusRequest* srAnimated,
                                                                        LedMode ledMode,
                                                                        uint8_t ledPin,
                                                                        bool rgbLed,
                                                                        uint32_t timeConstant,
                                                                        uint8_t ledHue,
                                                                        bool randomHue,
                                                                        QueueHandle_t xQueueLedState)
              : srBusy(srBusy), srAnimated(srAnimated), ledMode(ledMode), ledPin(ledPin), rgbLed(rgbLed), timeConstant(timeConstant), ledHue(ledHue), randomHue(randomHue), xQueueLedState(xQueueLedState) {
          }
      };

      void _initializeLedCallback();
      void _setLedCallback(LedMode ledMode, uint8_t ledHue, bool randomHue, uint32_t timeConstant);
      Task* _ledStateTask;
      void _ledStateCallback();
      LEDStateCallback _externalLedStateCallback = nullptr;
      static void _async_setLedTask(void* pvParameters);
      static void _adjustLed(CRGB* led, const CRGB& adjustment);
      StatusRequest _srInitialized;
      StatusRequest _srBusy;
      StatusRequest _srAnimated;
      Scheduler* _scheduler;
      QueueHandle_t _xQueueLedState;
      LedMode _ledMode;
      uint8_t _ledHue;
      bool _ledOn;
      uint8_t _ledPin;
      bool _rgbLed;
      TaskHandle_t _async_task_handle;
  };
} // namespace Soylent
