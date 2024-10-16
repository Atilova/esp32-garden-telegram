#pragma once

namespace AppConstants
    {
        namespace MegaCodes
            {
                constexpr char INET_REQUEST[] = "2560ask?:inet";
                constexpr char NTP_REQUEST[] = "2560ask?:ntp";
                constexpr char INET_OK[] = "inet.ok";
                constexpr char INET_NO[] = "inet.no";

                constexpr char EVENT_TYPE[] = "2560event!:";
                constexpr char RESPONSE_TYPE[] = "2560response!:";
            }

        namespace ChipCodes
            {
                constexpr char CHIP_PING_TYPE[] = "chip!:ping";
            }

        namespace WebServerEvents
            {
                constexpr char PING_MESSAGE[] = "ping";
                constexpr char USER_MESSAGE[] = "newUserMessage";
                constexpr char MEGA_MESSAGE[] = "newMegaMessage";
            }

        namespace UserMessages
            {
                constexpr char ESP_ONLINE[] = "🟢 Esp online";
                constexpr char NTP_SYNC_FAIL[] = "NTP: Ошибка синхронизации времени";
                constexpr char ESP_REBOOT[] = "🛑 Перезагрузка ESP32, ожидайте...";
            }

        namespace UserCommands
            {
                constexpr char HELP[] = "/help";
                constexpr char HELP_POWER[] = "/helppower";
                constexpr char MEMORY[] = "memory";
                constexpr char REBOOT[] = "reboot esp";
            }

        namespace InputPatterns
            {
                constexpr char CHIP_PING_PATTERN[] = "chip?:ping:%[^:]:null";
            }

        namespace OutputTemplates
            {
                constexpr char CHIP_PING_OK_TEMPLATE[] = "chip!:ping:%s:ok";
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
