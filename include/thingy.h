// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 Robert Wendlandt
 */
#pragma once

#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <MycilaESPConnect.h>

#include <WebServerTask.h>
#include <WebSiteTask.h>
#include <ESPRestartTask.h>
#include <EventHandlerTask.h>
#include <ESPConnectTask.h>
#include <LedTask.h>

// in main.cpp
extern Soylent::ESPRestartClass ESPRestart;
extern Soylent::ESPConnectClass ESPConnect;
extern Soylent::EventHandlerClass EventHandler;
extern Soylent::WebServerClass WebServer;
extern Soylent::WebSiteClass WebSite;
extern Soylent::LedClass Led;

// Spinlock for critical sections
extern portMUX_TYPE cs_spinlock;

namespace Soylent {
    template <class Container>
    void split_string(const std::string& str, Container& cont, const std::string& delims = " ")
    {
        std::size_t current, previous = 0;
        current = str.find_first_of(delims);
        while (current != std::string::npos) {
            cont.push_back(str.substr(previous, current - previous));
            previous = current + 1;
            current = str.find_first_of(delims, previous);
        }
        cont.push_back(str.substr(previous, current - previous));
    };
}

// Shorthands for Logging
#ifdef THINGY_DEBUG
    #ifdef MYCILA_LOGGER_SUPPORT
        #include <MycilaLogger.h>
        extern Mycila::Logger logger;
        #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
        #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
        #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
        #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
    #else
        #define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
        #define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
        #define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
        #define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
    #endif
    #else
        #define LOGD(tag, format, ...)
        #define LOGI(tag, format, ...)
        #define LOGW(tag, format, ...)
        #define LOGE(tag, format, ...)
#endif
