// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <thingy.h>
#define TAG "LED"

// default to LED_BUILTIN
Soylent::LedClass::LedClass()
    : _scheduler(nullptr)
    , _ledState(Soylent::LedClass::LedState::NONE)
    , _blinkIntervall(500)
    , _async_blink_task_handle(nullptr)
    , _async_params(nullptr) {    
    _srBusy.setWaiting();
    _srInitialized.setWaiting(); 
    _srBlinking.signalComplete();

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
    : _scheduler(nullptr)
    , _ledState(Soylent::LedClass::LedState::NONE)
    , _blinkIntervall(500)
    , _async_blink_task_handle(nullptr)
    , _async_params(nullptr) {    
    _srBusy.setWaiting();
    _srInitialized.setWaiting(); 
    _srBlinking.completed();

    _ledPin = LED_Pin; 
    _rgbLed = is_RGB;
}

void Soylent::LedClass::begin(Scheduler* scheduler) {
    _srBusy.setWaiting();
    _srInitialized.setWaiting();
    _srBlinking.signalComplete();
    _scheduler = scheduler; 
    _ledState = Soylent::LedClass::LedState::NONE;
    _blinkIntervall = 500;
    
    // create and run a task for initializing the LED
    Task* initializeLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _initializeLedCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    initializeLedTask->enable();

    LOGD(TAG, "LED is scheduled for start...");
}

void Soylent::LedClass::end() {

    LOGD(TAG, "Shutting down LED...");
    if (_async_blink_task_handle != nullptr) {
        vTaskDelete(_async_blink_task_handle);
        _async_blink_task_handle = nullptr;
    }

    if (digitalRead(_ledPin) && _ledPin != -1) {
        digitalWrite(_ledPin, LOW);
    }   

    _srBusy.setWaiting();
    _srInitialized.setWaiting(); 
    _srBlinking.signalComplete();
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

bool Soylent::LedClass::isBlinking() {
    return _srBlinking.pending();
} 

// set the LED state in a FreeRTOS-Task
void Soylent::LedClass::_async_setLedTask(void* pvParameters) {
    auto params = static_cast<Soylent::LedClass::async_params*>(pvParameters);
    // copy params (as they might be gone during blinking)
    auto led_Pin = params->ledPin;
    auto rgb_Led = params->rgbLed;
    auto blink_Intervall = params->blinkIntervall;

    // No LED present
    if (led_Pin == -1) {
        taskENTER_CRITICAL(&cs_spinlock);
        params->srBusy->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock); 

        vTaskDelete(NULL);
    }

    // run a blinking task?
    if (params->ledState == Soylent::LedClass::LedState::BLINK) {
        uint8_t red, green, blue;

        taskENTER_CRITICAL(&cs_spinlock);
        params->srBlinking->setWaiting();
        params->srBusy->signalComplete();
        taskEXIT_CRITICAL(&cs_spinlock);

        for (;;) {
            if (rgb_Led) {
                rgbLedWrite(led_Pin, 0, 0, 0);
            } else {
                digitalWrite(led_Pin, LOW);    
            }                   
            vTaskDelay(blink_Intervall);
            if (rgb_Led) {
                red = random(0, 128);
                green = random(0, 128 - red);
                blue = 128 - red - green;
                rgbLedWrite(led_Pin, red, green, blue);
            } else {
                digitalWrite(led_Pin, HIGH);
            }
            vTaskDelay(blink_Intervall);
        }

    } else {
        if (rgb_Led) {
            if (params->ledState == Soylent::LedClass::LedState::OFF) {
                rgbLedWrite(led_Pin, 0, 0, 0);
            } else {
                uint8_t red, green, blue;
                red = random(0, 128);
                green = random(0, 128 - red);
                blue = 128 - red - green;
                rgbLedWrite(led_Pin, red, green, blue);
            }
        } else {
            digitalWrite(led_Pin, (params->ledState == Soylent::LedClass::LedState::ON) ? HIGH : LOW);
        }
    }

    taskENTER_CRITICAL(&cs_spinlock);
    params->srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);    

    vTaskDelete(NULL);
}

void Soylent::LedClass::_setLedCallback() {
    _srBusy.setWaiting();

    // possibly stop the blinking task first
    if (_srBlinking.pending()) {
        if (_async_blink_task_handle != nullptr) {
            vTaskDelete(_async_blink_task_handle);
            _async_blink_task_handle = nullptr;
        }
        _srBlinking.signalComplete();
    }

    // Assemble the async parameters
    _async_params = static_cast<Soylent::LedClass::async_params*>(malloc(sizeof(async_params)));
    if (_async_params != NULL) {
        _async_params->srBusy = &_srBusy;
        _async_params->srBlinking = &_srBlinking;
        _async_params->ledState = _ledState;
        _async_params->ledPin = _ledPin;
        _async_params->rgbLed = _rgbLed;
        _async_params->blinkIntervall = _blinkIntervall;

        customTaskCreateUniversal(_async_setLedTask, "setLedTask", CONFIG_THINGY_TASKS_STACK_SIZE, 
            static_cast<void*>(_async_params), tskIDLE_PRIORITY + 1, 
            _ledState == Soylent::LedClass::LedState::BLINK ? &_async_blink_task_handle : NULL, 
            CONFIG_THINGY_TASKS_RUNNING_CORE);

        // not just for debugging...
        Task* report_async_setLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, 
            [&] { 
                    LOGD(TAG, "...async Led setting done!"); 
                    free(static_cast<void*>(_async_params)); 
                    _async_params = nullptr; 
                }, _scheduler, false, NULL, NULL, true);   
        report_async_setLedTask->enable();
        report_async_setLedTask->waitFor(&_srBusy);
    } else {
        LOGE(TAG, "Not enough memory for allocation!");
    }    
}

void Soylent::LedClass::setLedState(LedState ledState) {
    if (_srInitialized.pending()) {
        LOGW(TAG, "uninitialized, can't do it!");
        return;
    }

    if (_ledState != ledState) {
        _ledState = ledState;

        // create and run a task for creating and running an async task for setting the LED...
        // task is pending until the LED is not busy
        LOGD(TAG, "Start setting LED...");
        Task* setLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _setLedCallback(); }, 
            _scheduler, false, NULL, NULL, true);   
        setLedTask->enable();
        setLedTask->waitFor(&_srBusy);
    }    
}
