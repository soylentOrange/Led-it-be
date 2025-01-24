// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024-2025 Robert Wendlandt
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
        // struct for passing parameters to async LED tasks
        struct LEDTaskParams {
            struct {
                StatusRequest* srBusy;
                StatusRequest* srBlinking;
                LedState ledState;
                uint8_t ledPin;
                bool rgbLed;
                uint32_t blinkIntervall;
            };

            /// Default constructor
            /// @warning Default values are UNITIALIZED!
            constexpr inline __attribute__((always_inline)) LEDTaskParams()
                : srBusy(nullptr)
                , srBlinking(nullptr)
                , ledState(LedState::NONE)
                , ledPin(0)
                , rgbLed(false)
                , blinkIntervall(0) { 
            }

            /// Allow construction from values
            constexpr inline __attribute__((always_inline)) LEDTaskParams(StatusRequest& srBusy, 
                StatusRequest& srBlinking,
                LedState& ledState,
                uint8_t ledPin,
                bool rgbLed,
                uint32_t blinkIntervall)
                : srBusy(& srBusy)
                , srBlinking(& srBlinking)
                , ledState(ledState)
                , ledPin(ledPin)
                , rgbLed(rgbLed)
                , blinkIntervall(blinkIntervall) {
            }

            /// Allow copy construction
            constexpr inline LEDTaskParams(const LEDTaskParams& rhs) noexcept 
                : srBusy(rhs.srBusy)
                , srBlinking(rhs.srBlinking)
                , ledState(rhs.ledState)
                , ledPin(rhs.ledPin)
                , rgbLed(rhs.rgbLed)
                , blinkIntervall(rhs.blinkIntervall) { 
            }

            /// Allow copy construction
            inline __attribute__((always_inline)) LEDTaskParams& operator= (const LEDTaskParams& rhs) = default;
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
        TaskHandle_t _async_blink_task_handle;
    };

    // FastLED's HSV to RGB implementation
    // Convert an HSV value to RGB using a visually balanced rainbow. 
    // This "rainbow" yields better yellow and orange than a straight
    // mathematical "spectrum".
    // ![FastLED 'Rainbow' Hue Chart](https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-rainbow-with-desc.jpg)
    void hsv2rgb_rainbow(const struct CHSV& hsv, struct CRGB& rgb);

    inline __attribute__((always_inline)) static void nscale8x3(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t scale) {
        uint16_t scale_fixed = scale + 1;
        r = (((uint16_t)r) * scale_fixed) >> 8;
        g = (((uint16_t)g) * scale_fixed) >> 8;
        b = (((uint16_t)b) * scale_fixed) >> 8;
    };

    // Representation of an HSV pixel (hue, saturation, value (aka brightness)).
    struct CHSV {
        union {
            struct {
                union {
                    /// Color hue.
                    /// This is an 8-bit value representing an angle around
                    /// the color wheel. Where 0 is 0°, and 255 is 358°.
                    uint8_t hue;
                    uint8_t h;  ///< @copydoc hue
                };
                union {
                    /// Color saturation.
                    /// This is an 8-bit value representing a percentage.
                    uint8_t saturation;
                    uint8_t sat;  ///< @copydoc saturation
                    uint8_t s;    ///< @copydoc saturation
                };
                union {
                    /// Color value (brightness).
                    /// This is an 8-bit value representing a percentage.
                    uint8_t value;
                    uint8_t val;  ///< @copydoc value
                    uint8_t v;    ///< @copydoc value
                };
            };
            /// Access the hue, saturation, and value data as an array.
            /// Where:
            /// * `raw[0]` is the hue
            /// * `raw[1]` is the saturation
            /// * `raw[2]` is the value
            uint8_t raw[3];
        };

        /// Array access operator to index into the CHSV object
        /// @param x the index to retrieve (0-2)
        /// @returns the CHSV::raw value for the given index
        inline __attribute__((always_inline)) uint8_t& operator[] (uint8_t x) {
            return raw[x];
        }

        /// @copydoc operator[]
        inline __attribute__((always_inline)) const uint8_t& operator[] (uint8_t x) const {
            return raw[x];
        }

        /// Default constructor
        /// @warning Default values are UNITIALIZED!
        constexpr inline __attribute__((always_inline)) CHSV()
            : hue(0)
            , saturation(0)
            , value(0) { 
        }

        /// Allow construction from hue, saturation, and value
        /// @param ih input hue
        /// @param is input saturation
        /// @param iv input value
        constexpr inline __attribute__((always_inline)) CHSV( uint8_t ih, uint8_t is, uint8_t iv)
            : hue(ih), saturation(is), value(iv) {
        }

        /// Allow copy construction
        constexpr inline CHSV(const CHSV& rhs) noexcept 
            : hue(rhs.hue)
            , saturation(rhs.saturation)
            , value(rhs.value) { 
        }

        /// Allow copy construction
        inline __attribute__((always_inline)) CHSV& operator= (const CHSV& rhs) = default;

        /// Assign new HSV values
        /// @param ih input hue
        /// @param is input saturation
        /// @param iv input value
        /// @returns reference to the CHSV object
        inline __attribute__((always_inline)) CHSV& setHSV(uint8_t ih, uint8_t is, uint8_t iv) {
            hue = ih;
            saturation = is;
            value = iv;
            return *this;
        }
    };

    /// Pre-defined hue values for CHSV objects
    typedef enum {
        HUE_RED = 0,       ///< Red (0°)
        HUE_ORANGE = 32,   ///< Orange (45°)
        HUE_YELLOW = 64,   ///< Yellow (90°)
        HUE_GREEN = 96,    ///< Green (135°)
        HUE_AQUA = 128,    ///< Aqua (180°)
        HUE_BLUE = 160,    ///< Blue (225°)
        HUE_PURPLE = 192,  ///< Purple (270°)
        HUE_PINK = 224     ///< Pink (315°)
    } HSVHue;

    /// Representation of an RGB pixel (Red, Green, Blue)
    struct CRGB {
        union {
            struct {
                union {
                    uint8_t r;    ///< Red channel value
                    uint8_t red;  ///< @copydoc r
                };
                union {
                    uint8_t g;      ///< Green channel value
                    uint8_t green;  ///< @copydoc g
                };
                union {
                    uint8_t b;     ///< Blue channel value
                    uint8_t blue;  ///< @copydoc b
                };
            };
            /// Access the red, green, and blue data as an array.
            /// Where:
            /// * `raw[0]` is the red value
            /// * `raw[1]` is the green value
            /// * `raw[2]` is the blue value
            uint8_t raw[3];
        };

        /// Array access operator to index into the CRGB object
        /// @param x the index to retrieve (0-2)
        /// @returns the CRGB::raw value for the given index
        inline __attribute__((always_inline)) uint8_t& operator[] (uint8_t x) {
            return raw[x];
        }

        /// @copydoc operator[]
        inline __attribute__((always_inline)) const uint8_t& operator[] (uint8_t x) const {
            return raw[x];
        }

        /// Default constructor
        /// @warning Default values are UNITIALIZED!
        inline __attribute__((always_inline)) CRGB() = default;

        /// Allow construction from red, green, and blue
        /// @param ir input red value
        /// @param ig input green value
        /// @param ib input blue value
        constexpr inline CRGB(uint8_t ir, uint8_t ig, uint8_t ib) noexcept
            : r(ir), g(ig), b(ib) {
        }

        /// Allow construction from a CHSV color
        CRGB(const CHSV& rhs) __attribute__((always_inline)) {
            hsv2rgb_rainbow( rhs, *this);
        }

        /// Allow assignment from one RGB struct to another
        inline __attribute__((always_inline)) CRGB& operator= (const CRGB& rhs)  = default;

        /// Allow assignment from red, green, and blue
        /// @param nr new red value
        /// @param ng new green value
        /// @param nb new blue value
        inline __attribute__((always_inline)) CRGB& setRGB (uint8_t nr, uint8_t ng, uint8_t nb) {
            r = nr;
            g = ng;
            b = nb;
            return *this;
        }

        /// Allow assignment from hue, saturation, and value
        /// @param hue color hue
        /// @param sat color saturation
        /// @param val color value (brightness)
        inline __attribute__((always_inline)) CRGB& setHSV (uint8_t hue, uint8_t sat, uint8_t val) {
            hsv2rgb_rainbow( CHSV(hue, sat, val), *this);
            return *this;
        }

        /// Allow assignment from just a hue.
        /// Saturation and value (brightness) are set automatically to max.
        /// @param hue color hue
        inline __attribute__((always_inline)) CRGB& setHue (uint8_t hue) {
            hsv2rgb_rainbow( CHSV(hue, 255, 255), *this);
            return *this;
        }

        /// Allow assignment from HSV color
        inline __attribute__((always_inline)) CRGB& operator= (const CHSV& rhs) {
            hsv2rgb_rainbow( rhs, *this);
            return *this;
        }

        /// Return a CRGB object that is a scaled down version of this object
        inline __attribute__((always_inline)) CRGB scale8(uint8_t scaledown ) {
            CRGB out = *this;
            nscale8x3( out.r, out.g, out.b, scaledown);
            return out;
        }
    };

    static inline __attribute__((always_inline)) uint8_t scale8(uint8_t i, uint8_t scale) {
        return (((uint16_t)i) * (1 + (uint16_t)(scale))) >> 8;
    }

    static inline __attribute__((always_inline)) uint8_t scale8_video(uint8_t i, uint8_t scale) {
        uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
        return j;
    }

} // namespace Soylent
