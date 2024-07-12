#pragma once

namespace AppConstants
    {
        namespace MegaCodes
            {
                constexpr char INET_REQUEST[] = "2560ask?:inet";
                constexpr char NTP_REQUEST[] = "2560ask?:ntp";
                constexpr char INET_OK[] = "inet.ok";
                constexpr char INET_NO[] = "inet.no";
            }

        namespace WebServerEvents
            {
                constexpr char PING_MESSAGE[] = "ping";
                constexpr char USER_MESSAGE[] = "newUserMessage";
                constexpr char MEGA_MESSAGE[] = "newMegaMessage";
            }   

        namespace UserMessages
            {
                constexpr char NTP_SYNC_FAIL[] = "NTP: Ошибка синхронизации времени";
                constexpr char ESP_REBOOT[] = "🛑 Перезагрузка ESP32, ожидайте...";
            }

        namespace UserCommands
            {
                constexpr char HELP[] = "/help";
                constexpr char HELP_POWER[] = "/helppower";
                constexpr char MEMORY[] = "memory";
                constexpr char REBOOT[] = "reboot";
            }

        enum class State
            {
                WAKE_WIFI,
                INITIATE_MQTT_CONNECTION,
                CONSUME_MQTT,
                CONSUME_WEB_SERVER
            };

        struct AuxilaryValues
            {
                char* tzEnvPointer = nullptr;
                bool hasInternetAccess = false;
            };
    }
