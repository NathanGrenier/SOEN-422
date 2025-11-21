#pragma once
#include <Arduino.h>

namespace MQTT
{
    namespace Topics
    {
        // Data Topic: bins/{id}/data
        inline String getData(const char *deviceId)
        {
            return String("bins/") + deviceId + "/data";
        }

        // Status Topic: bins/{id}/status
        inline String getStatus(const char *deviceId)
        {
            return String("bins/") + deviceId + "/status";
        }

        // Ping Topic: cmd/ping/{id}
        inline String getPing(const char *deviceId)
        {
            return String("cmd/ping/") + deviceId;
        }
    }
}