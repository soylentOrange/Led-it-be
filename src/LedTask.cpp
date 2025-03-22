// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024-2025 Robert Wendlandt
 */

#include <thingy.h>
#include <memory>
#include <utility>
#define TAG "LED"

// default to LED_BUILTIN
Soylent::LedClass::LedClass()
    : _ledStateTask(nullptr), _scheduler(nullptr), _ledMode(Soylent::LedClass::LedMode::NONE), _ledOn(false), _ledHue(0), _async_task_handle(nullptr) {
  _srBusy.setWaiting();
  _srInitialized.setWaiting();
  _srAnimated.signalComplete();
  _xQueueLedState = xQueueCreate(10, sizeof(LedState));

#ifdef LED_BUILTIN
  _ledPin = LED_BUILTIN;
  #ifdef RGB_BUILTIN
  _rgbLed = true;
  #else
  _rgbLed = false;
  #endif
#else
  _ledPin = -1;
  _rgbLed = false;
#endif
}

Soylent::LedClass::LedClass(uint8_t LED_Pin, bool is_RGB)
    : _ledStateTask(nullptr), _scheduler(nullptr), _ledMode(Soylent::LedClass::LedMode::NONE), _ledOn(false), _ledHue(0), _async_task_handle(nullptr), _ledPin(LED_Pin), _rgbLed(is_RGB) {
  _srBusy.setWaiting();
  _srInitialized.setWaiting();
  _srAnimated.completed();
  _xQueueLedState = xQueueCreate(10, sizeof(LedState));
}

void Soylent::LedClass::begin(Scheduler* scheduler) {
  _srBusy.setWaiting();
  _srInitialized.setWaiting();
  _srAnimated.signalComplete();
  _scheduler = scheduler;
  _ledMode = Soylent::LedClass::LedMode::NONE;
  _ledOn = false;
  _ledHue = 0;

  // run a task for receiving LED state changes
  _ledStateTask = new Task(50, TASK_FOREVER, [&] { _ledStateCallback(); }, _scheduler, false, NULL, NULL, true);
  _ledStateTask->enable();

  // create and run a task for initializing the LED
  Task* initializeLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _initializeLedCallback(); }, _scheduler, false, NULL, NULL, true);
  initializeLedTask->enable();

  LOGD(TAG, "LED is scheduled for start...");
}

void Soylent::LedClass::end() {
  LOGD(TAG, "Shutting down LED...");
  if (_async_task_handle != nullptr) {
    vTaskDelete(_async_task_handle);
    _async_task_handle = nullptr;
  }

  if (digitalRead(_ledPin) && _ledPin != -1) {
    digitalWrite(_ledPin, LOW);
  }
  _ledStateTask->disable();
  _srBusy.setWaiting();
  _srInitialized.setWaiting();
  _srAnimated.signalComplete();
  LOGD(TAG, "...done!");
}

static bool customTaskCreateUniversal(
  TaskFunction_t pxTaskCode,
  const char* const pcName,
  const uint32_t usStackDepth,
  void* const pvParameters,
  UBaseType_t uxPriority,
  TaskHandle_t* const pxCreatedTask,
  const BaseType_t xCoreID) {
#ifndef CONFIG_FREERTOS_UNICORE
  if (xCoreID >= 0 && xCoreID < 2) {
    return xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID);
  } else {
#endif
    return xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
#ifndef CONFIG_FREERTOS_UNICORE
  }
#endif
}

// Receive state changes from the LED
void Soylent::LedClass::_ledStateCallback() {
  LedState rxLedState;
  if (xQueueReceive(_xQueueLedState, &rxLedState, (TickType_t)0) == pdPASS) {

    // Update private class variables
    _ledHue = rxLedState.ledHue;
    _ledOn = rxLedState.ledOn;

    // // make sure callback is called before auto restart
    // if (_externalLedStateCallback != nullptr)
    //   _externalLedStateCallback(LedState(_ledMode, _ledHue, _ledOn), rxLedState);

    LOGD(TAG, "LedMode: %d", _ledMode);
    if (rxLedState.ledOn) {
      LOGD(TAG, "Hue: %d", rxLedState.ledHue);
    } else {
      LOGD(TAG, "LED is off");
    }
  }
}

// Initialize the LED
void Soylent::LedClass::_initializeLedCallback() {
  LOGD(TAG, "Initialize LED...");

  // set LED-pin to out?
  _ledMode = Soylent::LedClass::LedMode::OFF;

  _srInitialized.signalComplete();
  LOGD(TAG, "...done!");

  // create and run a task for creating and running an async task for setting the LED...
  LOGD(TAG, "setting LED to off...");
  _setLedCallback(Soylent::LedClass::LedMode::OFF, 0, false, 0);
}

bool Soylent::LedClass::isInitialized() {
  return _srInitialized.completed();
}

bool Soylent::LedClass::isBusy() {
  return _srBusy.pending();
}

bool Soylent::LedClass::isAnimated() {
  return _srAnimated.pending();
}

void Soylent::LedClass::_adjustLed(CRGB* led, const CRGB& adjustment) {
  led->red = scale8(led->red, adjustment.red);
  led->green = scale8(led->green, adjustment.green);
  led->blue = scale8(led->blue, adjustment.blue);
}

// set the LED state in a FreeRTOS-Task
void Soylent::LedClass::_async_setLedTask(void* pvParameters) {
  auto params = static_cast<Soylent::LedClass::LEDTaskParams*>(pvParameters);
  // the LEDTaskParams are free'd after signalling _srBusy as complete
  // copy params (as they will be gone otherwise during blinking)
  LedMode ledMode = params->ledMode;
  uint8_t ledPin = params->ledPin;
  bool rgbLed = params->rgbLed;
  uint32_t timeConstant = params->timeConstant;
  uint8_t ledHue = params->ledHue;
  bool randomHue = params->randomHue;
  QueueHandle_t xQueueLedState = params->xQueueLedState;

  LOGD(TAG, "tc: %d", timeConstant);
  // adjust timeConstant to ticks
  timeConstant /= portTICK_PERIOD_MS;
  LOGD(TAG, "hue: %d", ledHue);
  LOGD(TAG, "mode: %d", ledMode);
  Soylent::LedClass::LedState ledState;
  ledState.ledHue = ledHue;

  // No LED present
  if (ledPin == -1) {
    taskENTER_CRITICAL(&cs_spinlock);
    params->srAnimated->signalComplete();
    params->srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);

    vTaskDelete(NULL);
  }

  // Calculate color adjustment
  CRGB led_colorAdjustment = CRGB::computeAdjustment(128, CRGB(255, 85, 210), CRGB(UncorrectedTemperature));

  // run a blinking task?
  if (ledMode == Soylent::LedClass::LedMode::BLINK) {
    taskENTER_CRITICAL(&cs_spinlock);
    params->srAnimated->setWaiting();
    params->srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);

    for (;;) {
      if (rgbLed) {
        rgbLedWrite(ledPin, 0, 0, 0);
      } else {
        digitalWrite(ledPin, LOW);
      }

      // Send out the message
      ledState.ledOn = false;
      xQueueSend(xQueueLedState, static_cast<void*>(&ledState), (TickType_t)0);

      // either just wait for time constant or terminate ourselves
      if (ulTaskNotifyTake(true, timeConstant) == TERMINATE_YOURSELF) {
        taskENTER_CRITICAL(&cs_spinlock);
        params->srAnimated->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock);
        digitalWrite(ledPin, LOW);

        vTaskDelete(NULL);
      }

      if (rgbLed) {
        // create a random color from the rainbow (at half brightness)
        if (randomHue) {
          ledHue = random(0, 255);
        }
        CRGB led_color(CHSV(ledHue, DEFAULT_SAT, DEFAULT_VALUE));
        _adjustLed(&led_color, led_colorAdjustment);
        rgbLedWrite(ledPin, led_color.red, led_color.green, led_color.blue);

        // Send out the message
        ledState.ledHue = ledHue;
        ledState.ledOn = true;
        xQueueSend(xQueueLedState, static_cast<void*>(&ledState), (TickType_t)0);
        // LOGD(TAG, "hue: %d (%d, %d, %d)", ledHue, led_color.red, led_color.green, led_color.blue);
      } else {
        digitalWrite(ledPin, HIGH);

        // Send out the message
        ledState.ledOn = true;
        xQueueSend(xQueueLedState, static_cast<void*>(&ledState), (TickType_t)0);
      }

      // either just wait for time constant or terminate ourselves
      if (ulTaskNotifyTake(true, timeConstant) == TERMINATE_YOURSELF) {
        taskENTER_CRITICAL(&cs_spinlock);
        params->srAnimated->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock);
        digitalWrite(ledPin, LOW);

        vTaskDelete(NULL);
      }
    }
  } else if (ledMode == Soylent::LedClass::LedMode::RAINBOW) {
    // run a rainbow task?
    taskENTER_CRITICAL(&cs_spinlock);
    params->srAnimated->setWaiting();
    params->srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);

    // Send out the message
    ledState.ledOn = true;
    xQueueSend(xQueueLedState, static_cast<void*>(&ledState), (TickType_t)0);

    for (;;) {
      if (rgbLed) {
        // loop through the the rainbow (at half brightness)
        CRGB led_color(CHSV(ledHue, DEFAULT_SAT, DEFAULT_VALUE));
        _adjustLed(&led_color, led_colorAdjustment);
        ledHue++;
        rgbLedWrite(ledPin, led_color.red, led_color.green, led_color.blue);
      } else {
        digitalWrite(ledPin, HIGH);
      }

      // either just wait for time constant or terminate ourselves
      if (ulTaskNotifyTake(true, timeConstant) == TERMINATE_YOURSELF) {
        taskENTER_CRITICAL(&cs_spinlock);
        params->srAnimated->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock);
        digitalWrite(ledPin, LOW);

        vTaskDelete(NULL);
      }
    }
  } else {
    // Just a one time setting here
    if (rgbLed) {
      if (ledMode == Soylent::LedClass::LedMode::OFF) {
        rgbLedWrite(ledPin, 0, 0, 0);

        // Send out the message
        ledState.ledOn = false;
        xQueueSend(xQueueLedState, static_cast<void*>(&ledState), (TickType_t)0);
      } else {
        if (randomHue) {
          // create a random color from the rainbow (at half brightness)
          ledHue = random(0, 255);
        }
        CRGB led_color(CHSV(ledHue, DEFAULT_SAT, DEFAULT_VALUE));
        _adjustLed(&led_color, led_colorAdjustment);
        rgbLedWrite(ledPin, led_color.red, led_color.green, led_color.blue);
        // Send out the message
        ledState.ledHue = ledHue;
        ledState.ledOn = true;
        xQueueSend(xQueueLedState, static_cast<void*>(&ledState), (TickType_t)0);
        LOGD(TAG, "hue: %d (%d, %d, %d)", ledHue, led_color.red, led_color.green, led_color.blue);
      }
    } else {
      digitalWrite(ledPin, (ledMode == Soylent::LedClass::LedMode::ON) ? HIGH : LOW);

      // Send out the message
      ledState.ledOn = (ledMode == Soylent::LedClass::LedMode::ON) ? true : false;
      xQueueSend(xQueueLedState, static_cast<void*>(&ledState), (TickType_t)0);
      LOGD(TAG, "LED %s!", (ledMode == Soylent::LedClass::LedMode::ON) ? "on" : "off");
    }
  }

  taskENTER_CRITICAL(&cs_spinlock);
  params->srAnimated->signalComplete();
  params->srBusy->signalComplete();
  taskEXIT_CRITICAL(&cs_spinlock);

  vTaskDelete(NULL);
}

void Soylent::LedClass::_setLedCallback(LedMode ledMode, uint8_t ledHue, bool randomHue, uint32_t timeConstant) {
  _srBusy.setWaiting();

  // possibly stop the blinking or rainbow task first
  if (_srAnimated.pending()) {
    if (_async_task_handle != nullptr) {
      xTaskNotify(_async_task_handle, TERMINATE_YOURSELF, eSetValueWithOverwrite);
      yield();
    }
  }

  // allocate and assemble the async parameters in a shared_ptr
  auto p = std::make_shared<LEDTaskParams>(&_srBusy, &_srAnimated, ledMode, _ledPin, _rgbLed, timeConstant, ledHue, randomHue, _xQueueLedState);
  if (p) {
    // create the FreeRTOS-Task
    customTaskCreateUniversal(_async_setLedTask, "setLedTask", CONFIG_THINGY_TASKS_STACK_SIZE,
                              // pass the underlying pointer to LEDTaskParams from within shared_ptr to the FreeRTOS-task
                              static_cast<void*>(p.get()),
                              tskIDLE_PRIORITY + 1,
                              &_async_task_handle,
                              CONFIG_THINGY_TASKS_RUNNING_CORE);

    // not just for debugging...
    Task* report_async_setLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE,
                                             // pass ownership of shared_ptr to LEDTaskParams to the new cooperative task
                                             [moved_LEDTaskParams = move(p)] {
                                               LOGD(TAG, "...async Led setting done!");
                                               // the LEDTaskParams will be deallocated after this task deletes itself
                                               // take care in _async_setLedTask to use the passed pointer to LEDTaskParams only as long _srBusy is pending
                                             },
                                             _scheduler,
                                             false,
                                             NULL,
                                             NULL,
                                             true);
    report_async_setLedTask->enable();
    report_async_setLedTask->waitFor(&_srBusy);
  } else {
    LOGE(TAG, "Not enough memory for allocation!");
  }
}

Soylent::LedClass::LedState Soylent::LedClass::getLedState() {
  return LedState(_ledHue, _ledOn);
}

Soylent::LedClass::LedMode Soylent::LedClass::getLedMode() {
  return _ledMode;
}

void Soylent::LedClass::setLed(LedMode ledMode, uint8_t ledHue, bool randomHue) {
  if (_srInitialized.pending()) {
    LOGW(TAG, "uninitialized, can't do it!");
    return;
  }

  // if ((_ledMode != ledAnimation) || (_ledHue != ledHue) || randomHue) {
  //   _ledMode = ledAnimation;
  //   _ledHue = ledHue;
  //   _randomHue = randomHue;
  //   yield();

  // if (_ledMode == LedMode::BLINK) {
  //   _timeConstant = 500;
  // } else if (_ledMode == LedMode::RAINBOW) {
  //   _timeConstant = 19;
  // }

  uint32_t timeConstant = 500;
  // if (ledMode == LedMode::BLINK) {
  //   timeConstant = 500;
  // } else if (ledMode == LedMode::RAINBOW) {
  //   timeConstant = 19;
  // }

  // store in private section of class
  _ledMode = ledMode;
  _ledHue = ledHue;

  // create and run a task for creating and running an async task for setting the LED...
  // task is pending until the LED is not busy
  LOGD(TAG, "Start setting LED...");
  LOGD(TAG, "hue: %d", ledHue);
  LOGD(TAG, "random: %d", randomHue);
  Task* setLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _setLedCallback(_ledMode, _ledHue, randomHue, 500); }, _scheduler, false, NULL, NULL, true);
  setLedTask->enable();
  setLedTask->waitFor(&_srBusy);
  // }
}

void Soylent::LedClass::setLedMode(LedMode ledMode) {
  Soylent::LedClass::setLed(ledMode, true, 0);
}
