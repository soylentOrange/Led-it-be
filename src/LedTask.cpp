// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024-2025 Robert Wendlandt
 */
#include <thingy.h>
#define TAG "LED"

// default to LED_BUILTIN
Soylent::LedClass::LedClass()
    : _scheduler(nullptr), _ledState(Soylent::LedClass::LedState::NONE), _timeConstant(500), _async_task_handle(nullptr) {
  _srBusy.setWaiting();
  _srInitialized.setWaiting();
  _srAnimated.signalComplete();

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
    : _scheduler(nullptr), _ledState(Soylent::LedClass::LedState::NONE), _timeConstant(500), _async_task_handle(nullptr), _ledPin(LED_Pin), _rgbLed(is_RGB) {
  _srBusy.setWaiting();
  _srInitialized.setWaiting();
  _srAnimated.completed();
}

void Soylent::LedClass::begin(Scheduler* scheduler) {
  _srBusy.setWaiting();
  _srInitialized.setWaiting();
  _srAnimated.signalComplete();
  _scheduler = scheduler;
  _ledState = Soylent::LedClass::LedState::NONE;
  _timeConstant = 500;

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

// Initialize the LED
void Soylent::LedClass::_initializeLedCallback() {
  LOGD(TAG, "Initialize LED...");

  // set LED-pin to out?
  _ledState = Soylent::LedClass::LedState::OFF;

  _srInitialized.signalComplete();
  LOGD(TAG, "...done!");

  // create and run a task for creating and running an async task for setting the LED...
  LOGD(TAG, "setting LED to off...");
  _setLedCallback();
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
  Soylent::LedClass::LEDTaskParams task_params(*params);
  // adjust timeConstant to ticks
  task_params.timeConstant /= portTICK_PERIOD_MS;

  // No LED present
  if (task_params.ledPin == -1) {
    taskENTER_CRITICAL(&cs_spinlock);
    task_params.srAnimated->signalComplete();
    task_params.srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);

    vTaskDelete(NULL);
  }

  // Calculate color adjustment
  CRGB led_colorAdjustment = CRGB::computeAdjustment(128, CRGB(255, 85, 210), CRGB(UncorrectedTemperature));

  // run a blinking task?
  if (task_params.ledState == Soylent::LedClass::LedState::BLINK) {
    taskENTER_CRITICAL(&cs_spinlock);
    task_params.srAnimated->setWaiting();
    task_params.srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);

    for (;;) {
      if (task_params.rgbLed) {
        rgbLedWrite(task_params.ledPin, 0, 0, 0);
      } else {
        digitalWrite(task_params.ledPin, LOW);
      }

      // either just wait for time constant or terminate ourselves
      if (ulTaskNotifyTake(true, task_params.timeConstant) == TERMINATE_YOURSELF) {
        taskENTER_CRITICAL(&cs_spinlock);
        task_params.srAnimated->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock);
        digitalWrite(task_params.ledPin, LOW);

        vTaskDelete(NULL);
      }

      if (task_params.rgbLed) {
        // create a random color from the rainbow (at half brightness)
        task_params.hue = random(0, 255);
        CRGB led_color(CHSV(task_params.hue, 240, 255));
        _adjustLed(&led_color, led_colorAdjustment);
        rgbLedWrite(task_params.ledPin, led_color.red, led_color.green, led_color.blue);
        LOGD(TAG, "hue: %d (%d, %d, %d)", task_params.hue, led_color.red, led_color.green, led_color.blue);
      } else {
        digitalWrite(task_params.ledPin, HIGH);
      }

      // either just wait for time constant or terminate ourselves
      if (ulTaskNotifyTake(true, task_params.timeConstant) == TERMINATE_YOURSELF) {
        taskENTER_CRITICAL(&cs_spinlock);
        task_params.srAnimated->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock);
        digitalWrite(task_params.ledPin, LOW);

        vTaskDelete(NULL);
      }
    }
  } else if (task_params.ledState == Soylent::LedClass::LedState::RAINBOW) {
    // run a rainbow task?
    taskENTER_CRITICAL(&cs_spinlock);
    task_params.srAnimated->setWaiting();
    task_params.srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);

    for (;;) {
      if (task_params.rgbLed) {
        // loop through the the rainbow (at half brightness)
        CRGB led_color(CHSV(task_params.hue, 240, 255));
        _adjustLed(&led_color, led_colorAdjustment);
        task_params.hue++;
        rgbLedWrite(task_params.ledPin, led_color.red, led_color.green, led_color.blue);
      } else {
        digitalWrite(task_params.ledPin, HIGH);
      }

      // either just wait for time constant or terminate ourselves
      if (ulTaskNotifyTake(true, task_params.timeConstant) == TERMINATE_YOURSELF) {
        taskENTER_CRITICAL(&cs_spinlock);
        task_params.srAnimated->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock);
        digitalWrite(task_params.ledPin, LOW);

        vTaskDelete(NULL);
      }
    }
  } else {
    // Just a one time setting here
    if (task_params.rgbLed) {
      if (task_params.ledState == Soylent::LedClass::LedState::OFF) {
        rgbLedWrite(task_params.ledPin, 0, 0, 0);
      } else {
        // create a random color from the rainbow (at half brightness)
        task_params.hue = random(0, 255);
        CRGB led_color(CHSV(task_params.hue, 240, 255));
        _adjustLed(&led_color, led_colorAdjustment);
        rgbLedWrite(task_params.ledPin, led_color.red, led_color.green, led_color.blue);
        LOGD(TAG, "hue: %d (%d, %d, %d)", task_params.hue, led_color.red, led_color.green, led_color.blue);
      }
    } else {
      digitalWrite(task_params.ledPin, (task_params.ledState == Soylent::LedClass::LedState::ON) ? HIGH : LOW);
      LOGD(TAG, "LED %s!", (task_params.ledState == Soylent::LedClass::LedState::ON) ? "on" : "off");
    }
  }

  taskENTER_CRITICAL(&cs_spinlock);
  task_params.srAnimated->signalComplete();
  task_params.srBusy->signalComplete();
  taskEXIT_CRITICAL(&cs_spinlock);

  vTaskDelete(NULL);
}

void Soylent::LedClass::_setLedCallback() {
  _srBusy.setWaiting();

  // possibly stop the blinking or rainbow task first
  if (_srAnimated.pending()) {
    if (_async_task_handle != nullptr) {
      xTaskNotify(_async_task_handle, TERMINATE_YOURSELF, eSetValueWithOverwrite);
      yield();
    }
  }

  // allocate and assemble the async parameters in a shared_ptr
  auto p = std::make_shared<LEDTaskParams>(&_srBusy, &_srAnimated, _ledState, _ledPin, _rgbLed, _timeConstant, 0);
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
  return _ledState;
}

void Soylent::LedClass::setLedState(LedState ledState) {
  if (_srInitialized.pending()) {
    LOGW(TAG, "uninitialized, can't do it!");
    return;
  }

  if (_ledState != ledState) {
    _ledState = ledState;

    if (_ledState == LedState::BLINK) {
      _timeConstant = 500;
    } else if (_ledState == LedState::RAINBOW) {
      _timeConstant = 19;
    }

    // create and run a task for creating and running an async task for setting the LED...
    // task is pending until the LED is not busy
    LOGD(TAG, "Start setting LED...");
    Task* setLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _setLedCallback(); }, _scheduler, false, NULL, NULL, true);
    setLedTask->enable();
    setLedTask->waitFor(&_srBusy);
  }
}
