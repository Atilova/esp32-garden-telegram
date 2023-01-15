#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "Secret.h"
#include "App.h"
#include <IPAddress.h>

#define len(object) sizeof(object) / sizeof(*object)

const IPAddress hosts[] =
    {
        IPAddress(8, 8, 8, 8),
        IPAddress(8, 8, 4, 4)
    };

const char* webButtons[][2] =
    {
        {"status", "btnStatus"},
        {"info", "btnInfo"},
        {"list", "btnList"},
        {"on", "btnOn"},
        {"shutdown", "btnShutdown"},
        {"stop", "btnStop"},
        {"tank", "btnTank"},
        {"active", "btnActive"},
        {"active", "btnActive"},
        {".deep water control", "btnDeepWaterControl"},
        {"options", "btnOptions"},
        {"/help", "btnHelp"}
    };

AppConfig conf {
    .WIFI_IP = IPAddress(192, 168, 1, 111),
    .WIFI_GATEWAY = IPAddress(192, 168, 1, 1),
    .WIFI_SUBNET_MASK = IPAddress(255, 255, 255, 0),
    .WIFI_PRIMARY_DNS = IPAddress(192, 168, 1, 1),
    .WIFI_SECONDARY_DNS = IPAddress(8, 8, 8, 8),
    .WIFI_SSID = secretConfig.WIFI_SSID,
    .WIFI_PASSWORD = secretConfig.WIFI_PASSWORD,
    .BOT_TOKEN = secretConfig.BOT_TOKEN,
    .ALLOWED_USER = secretConfig.ALLOWED_USER,
    .PING_HOSTS = hosts,
    .PING_HOSTS_LENGTH = len(hosts),
    .WEB_SERVER_PORT = 80,
    .WEB_BUTTONS = webButtons,
    .WEB_BUTTONS_LENGTH = len(webButtons),
    .ESP_LED_PIN = 2
};
#endif