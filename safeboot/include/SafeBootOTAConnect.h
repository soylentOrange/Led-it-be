// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou, 2024 Robert Wendlandt
 */
#pragma once

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <string>
#include <Ticker.h>

class SafeBootOTAConnect {
    public:
        enum class State {
            // Starting up ESP => NETWORK_DISABLED
            NETWORK_DISABLED = 0,
            // NETWORK_DISABLED => NETWORK_ENABLED
            NETWORK_ENABLED,
            // NETWORK_ENABLED => NETWORK_CONNECTING
            NETWORK_CONNECTING,
            // NETWORK_CONNECTING => NETWORK_TIMEOUT
            NETWORK_TIMEOUT,
            // NETWORK_CONNECTING => NETWORK_CONNECTED
            // NETWORK_RECONNECTING => NETWORK_CONNECTED
            NETWORK_CONNECTED, // semi-final state ==> start OTA-services
            // NETWORK_CONNECTED => NETWORK_DISCONNECTED
            NETWORK_DISCONNECTED,
            // NETWORK_DISCONNECTED => NETWORK_RECONNECTING
            NETWORK_RECONNECTING,

            // NETWORK_ENABLED => AP_STARTING
            AP_STARTING,
            // AP_STARTING => AP_STARTED
            AP_STARTED, // semi-final state ==> start OTA-services

            // NETWORK_CONNECTED => OTA_UPDATER_STARTING
            // AP_STARTED => OTA_UPDATER_STARTING
            OTA_UPDATER_STARTING, 
            // OTA_UPDATER_STARTING => OTA_UPDATER_STARTED
            OTA_UPDATER_STARTED, // final state ==> OTA-Website is ready
            // OTA_UPDATER_STARTED => OTA_UPDATER_TIMEOUT
            OTA_UPDATER_TIMEOUT, // ==> bail-out...
        };

        typedef std::function<void(State previous, State state)> StateCallback;

        typedef struct {
            // Static IP address to use when connecting to WiFi (STA mode)
            // If not set, DHCP will be used
            IPAddress ip;
            // Subnet mask: 255.255.255.0
            IPAddress subnet;
            IPAddress gateway;
            IPAddress dns;
        } IPConfig;

        typedef struct {
            // SSID name to connect to, loaded from config or set from begin()
            std::string wifiSSID;
            // Password for the WiFi to connect to, loaded from config or set from begin()
            std::string wifiPassword;
            // whether we need to set the ESP to stay in AP mode or not, loaded from config, begin()
            bool apMode;
        } Config;

        typedef struct {
            // port to use
            uint16_t port;
            // reboot on success
            bool rebootOnSuccess;
            // password to use
            std::string password;
            // whether ArduinoOTA should be made available or not
            bool enableArduinoOTA;
        } ArduinoOTAConfig;

        enum class OTAMode {
            Firmware,
            Filesystem,
        };

    public:
        explicit SafeBootOTAConnect(AsyncWebServer& httpd) : _httpd(&httpd) {}
        ~SafeBootOTAConnect() { end(); }

        // Start SafeBootOTAConnect:
        //
        // 1. Load the configuration (which has to be set in the main app's preferences via "espconnect")
        // 2. If apMode is true, starts in AP Mode
        // 3. If apMode is false, try to start in STA mode
        // 4. If STA mode times out, or nothing configured, starts in AP Mode
        // 5. The OTA-Update services will be started
        //
        // Using this method will activate auto-load of the configuration
        void begin(const char* hostname, const char* apSSID, const char* apPassword = ""); // NOLINT

        // Start SafeBootOTAConnect:
        //
        // 1. If apMode is true, starts in AP Mode
        // 2. If apMode is false, try to start in STA mode
        // 3. If STA mode fails, or empty WiFi credentials were passed, starts in AP Mode
        // 5. The OTA-Update services will be started
        //
        // Using this method will NOT auto-load any configuration
        void begin(const char* hostname, const char* apSSID, const char* apPassword, const Config& config); // NOLINT

        // loop() method to be called from main loop()
        void loop();

        // Stops the network stack
        void end();

        // Listen for network state change
        void listen(StateCallback callback) { _callback = callback; }

        // Static IP configuration: by default, DHCP is used
        // The static IP configuration applies to the WiFi STA connection.
        // Warning: use only before begin!
        void setIPConfig(const IPConfig& ipConfig) { _ipConfig = ipConfig; }

        // ArduinoOTA configuration: by default, defines from platformio.ini are used
        // Warning: use only before begin!
        void setArduinoOTAConfig(const ArduinoOTAConfig& arduinoOTAConfig) { _arduinoOTAConfig = arduinoOTAConfig; }

        // set OTA-Mode (Firmware or Filesystem)
        void setOTAMode(OTAMode otaMode);

    private:
        AsyncWebServer* _httpd = nullptr;
        State _state = State::NETWORK_DISABLED;
        StateCallback _callback = nullptr;
        DNSServer* _dnsServer = nullptr;
        int64_t _lastTime = -1;
        std::string _hostname;
        std::string _apSSID;
        std::string _apPassword;
        uint32_t _connectTimeout = TIMEOUT_CONNECT;
        uint32_t _otaServicesTimeout = TIMEOUT_OTA_UPDATER;
        Config _config;
        ArduinoOTAConfig _arduinoOTAConfig = {ARDUINOOTA_PORT, true, "", true};
        IPConfig _ipConfig;
        WiFiEventId_t _wifiEventListenerId = 0;
        Ticker _delayedTask;
        uint32_t _delayBeforeRestart = 0;
        uint32_t _hotspotDetectCounter = 0;
        uint32_t _otaMode = 0; 
        std::string _otaResultString;
        std::string _boardName;
        uint32_t _logo_len;
        uint8_t* _logo;

    private:
        void _startSTA();
        void _startAP();
        void _enableOTAServices();
        void _onWiFiEvent(WiFiEvent_t event);
        bool _durationPassed(uint32_t intervalSec);
        void _restartDelayed(uint32_t msDelayBeforeCleanup = 500, uint32_t msDelayBeforeRestart = 500);
        void _doCleanupAndRestart();
        void _doCleanup();
        void _onArduinoOTAStarted();
        void _setState(State state);
};
