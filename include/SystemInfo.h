// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <esp_system.h>
#include <map>
#include <string>

namespace Soylent {
  class SystemInfoClass {
    public:
      SystemInfoClass() {};

      // Get Reset reason as String
      std::string getResetReasonString() {
        if (auto resetReason = _resetReasonMap.find(esp_reset_reason()); resetReason != _resetReasonMap.end()) {
          return resetReason->second;
        } else {
          return _resetReasonMap[ESP_RST_UNKNOWN];
        }
      }

    private:
      std::map<esp_reset_reason_t, std::string> _resetReasonMap = {
        {ESP_RST_UNKNOWN, "Reset reason cannot be determined"},
        {ESP_RST_POWERON, "Reset due to power-on event"},
        {ESP_RST_EXT, "Reset by external pin"},
        {ESP_RST_SW, "Software reset via esp_restart"},
        {ESP_RST_PANIC, "Software reset due to exception/panic"},
        {ESP_RST_INT_WDT, "Reset (software or hardware) due to interrupt watchdog"},
        {ESP_RST_TASK_WDT, "Reset due to task watchdog"},
        {ESP_RST_WDT, "Reset due to other watchdogs"},
        {ESP_RST_DEEPSLEEP, "Reset after exiting deep sleep mode"},
        {ESP_RST_BROWNOUT, "Brownout reset (software or hardware)"},
        {ESP_RST_SDIO, "Reset over SDIO"},
        {ESP_RST_USB, "Reset by USB peripheral"},
        {ESP_RST_JTAG, "Reset by JTAG"},
        {ESP_RST_EFUSE, "Reset due to efuse error"},
        {ESP_RST_PWR_GLITCH, "Reset due to power glitch detected"},
        {ESP_RST_CPU_LOCKUP, "Reset due to CPU lock up (double exception)"},
      };
  };
}

Soylent::SystemInfoClass SystemInfo;
