// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024-2025 Robert Wendlandt
 */
#include <thingy.h>
#define TAG "LED"

// default to LED_BUILTIN
Soylent::LedClass::LedClass()
    : _scheduler(nullptr)
    , _ledState(Soylent::LedClass::LedState::NONE)
    , _blinkIntervall(500)
    , _async_blink_task_handle(nullptr) {    
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
    , _async_blink_task_handle(nullptr) {    
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

/// Force a variable reference to avoid compiler over-optimization. 
/// Sometimes the compiler will do clever things to reduce
/// code size that result in a net slowdown, if it thinks that
/// a variable is not used in a certain location.
/// This macro does its best to convince the compiler that
/// the variable is used in this location, to help control
/// code motion and de-duplication that would result in a slowdown.
#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )

/// @cond
#define K255 255
#define K171 171
#define K170 170
#define K85  85
/// @endcond

void Soylent::hsv2rgb_rainbow( const Soylent::CHSV& hsv, Soylent::CRGB& rgb)
{
    // Yellow has a higher inherent brightness than
    // any other color; 'pure' yellow is perceived to
    // be 93% as bright as white.  In order to make
    // yellow appear the correct relative brightness,
    // it has to be rendered brighter than all other
    // colors.
    // Level Y1 is a moderate boost, the default.
    // Level Y2 is a strong boost.
    const uint8_t Y1 = 1;
    const uint8_t Y2 = 0;
    
    // G2: Whether to divide all greens by two.
    // Depends GREATLY on your particular LEDs
    const uint8_t G2 = 0;
    
    // Gscale: what to scale green down by.
    // Depends GREATLY on your particular LEDs
    const uint8_t Gscale = 0;
    
    uint8_t hue = hsv.hue;
    uint8_t sat = hsv.sat;
    uint8_t val = hsv.val;
    
    uint8_t offset = hue & 0x1F; // 0..31
    
    // offset8 = offset * 8
    uint8_t offset8 = offset;
    offset8 <<= 3;
    
    uint8_t third = scale8( offset8, (256 / 3)); // max = 85
    
    uint8_t r, g, b;
    
    if( ! (hue & 0x80) ) {
        // 0XX
        if( ! (hue & 0x40) ) {
            // 00X
            //section 0-1
            if( ! (hue & 0x20) ) {
                // 000
                //case 0: // R -> O
                r = K255 - third;
                g = third;
                b = 0;
                FORCE_REFERENCE(b);
            } else {
                // 001
                //case 1: // O -> Y
                if( Y1 ) {
                    r = K171;
                    g = K85 + third ;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
                if( Y2 ) {
                    r = K170 + third;
                    //uint8_t twothirds = (third << 1);
                    uint8_t twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                    g = K85 + twothirds;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            }
        } else {
            //01X
            // section 2-3
            if( !  (hue & 0x20) ) {
                // 010
                //case 2: // Y -> G
                if( Y1 ) {
                    //uint8_t twothirds = (third << 1);
                    uint8_t twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                    r = K171 - twothirds;
                    g = K170 + third;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
                if( Y2 ) {
                    r = K255 - offset8;
                    g = K255;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            } else {
                // 011
                // case 3: // G -> A
                r = 0;
                FORCE_REFERENCE(r);
                g = K255 - third;
                b = third;
            }
        }
    } else {
        // section 4-7
        // 1XX
        if( ! (hue & 0x40) ) {
            // 10X
            if( ! ( hue & 0x20) ) {
                // 100
                //case 4: // A -> B
                r = 0;
                FORCE_REFERENCE(r);
                //uint8_t twothirds = (third << 1);
                uint8_t twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                g = K171 - twothirds; //K170?
                b = K85  + twothirds;
                
            } else {
                // 101
                //case 5: // B -> P
                r = third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K255 - third;
                
            }
        } else {
            if( !  (hue & 0x20)  ) {
                // 110
                //case 6: // P -- K
                r = K85 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K171 - third;
                
            } else {
                // 111
                //case 7: // K -> R
                r = K170 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K85 - third;                
            }
        }
    }
    
    // This is one of the good places to scale the green down,
    // although the client can scale green down as well.
    if (G2) 
        g = g >> 1;
    if (Gscale) 
        g = scale8_video(g, Gscale);
    
    // Scale down colors if we're desaturated at all
    // and add the brightness_floor to r, g, and b.
    if (sat != 255) {
        if (sat == 0) {
            r = 255; b = 255; g = 255;
        } else {
            uint8_t desat = 255 - sat;
            desat = scale8_video( desat, desat);

            uint8_t satscale = 255 - desat;
            
            r = scale8( r, satscale);
            g = scale8( g, satscale);
            b = scale8( b, satscale);

            uint8_t brightness_floor = desat;
            r += brightness_floor;
            g += brightness_floor;
            b += brightness_floor;
        }
    }
    
    // Now scale everything down if we're at value < 255.
    if (val != 255) {
        
        val = scale8_video( val, val);
        if (val == 0) {
            r=0; g=0; b=0;
        } else {
            // nscale8x3_video( r, g, b, val);
            r = scale8( r, val);
            g = scale8( g, val);
            b = scale8( b, val);
        }
    }
    
    // Here we have the old AVR "missing std X+n" problem again
    // It turns out that fixing it winds up costing more than
    // not fixing it.
    // To paraphrase Dr Bronner, profile! profile! profile!
    //asm volatile(  ""  :  :  : "r26", "r27" );
    //asm volatile (" movw r30, r26 \n" : : : "r30", "r31");
    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
}

// set the LED state in a FreeRTOS-Task
void Soylent::LedClass::_async_setLedTask(void* pvParameters) {
    auto params = static_cast<Soylent::LedClass::LEDTaskParams*>(pvParameters);
    // the LEDTaskParams are free'd after signalling _srBusy as complete
    // copy params (as they will be gone otherwise during blinking)
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
                // create a random color from the rainbow (at half brightness)
                uint8_t random_hue = random(0, 255);
                CRGB led_color(CHSV(random_hue, 255, 128));
                rgbLedWrite(led_Pin, led_color.red, led_color.green, led_color.blue);
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
                // create a random color from the rainbow (at half brightness)
                uint8_t random_hue = random(0, 255);
                CRGB led_color(CHSV(random_hue, 255, 128));
                rgbLedWrite(led_Pin, led_color.red, led_color.green, led_color.blue);
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

    // allocate and assemble the async parameters in a shared_ptr
    auto p = std::make_shared<LEDTaskParams>(_srBusy, _srBlinking, _ledState, 
        _ledPin, _rgbLed, _blinkIntervall);
    if (p) {

        customTaskCreateUniversal(_async_setLedTask, "setLedTask", CONFIG_THINGY_TASKS_STACK_SIZE, 
            // pass the underlying pointer to LEDTaskParams from within shared_ptr to the FreeRTOS-task 
            static_cast<void*>(p.get()), 
            tskIDLE_PRIORITY + 1, 
            _ledState == Soylent::LedClass::LedState::BLINK ? &_async_blink_task_handle : NULL, 
            CONFIG_THINGY_TASKS_RUNNING_CORE);

        // not just for debugging...
        Task* report_async_setLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, 
            // pass ownership of shared_ptr to LEDTaskParams to the new cooperative task
            [moved_LEDTaskParams = move(p)] { 
                    LOGD(TAG, "...async Led setting done!"); 
                    // the LEDTaskParams will be deallocated while this task deletes itself
                    // take care in _async_setLedTask to use the passed pointer to LEDTaskParams only as long _srBusy is pending
                    LOGD(TAG, "free_heap: %d", esp_get_free_heap_size());
                }, _scheduler, false, NULL, NULL, true);   
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

        // create and run a task for creating and running an async task for setting the LED...
        // task is pending until the LED is not busy
        LOGD(TAG, "Start setting LED...");
        Task* setLedTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _setLedCallback(); }, 
            _scheduler, false, NULL, NULL, true);   
        setLedTask->enable();
        setLedTask->waitFor(&_srBusy);
    }    
}
