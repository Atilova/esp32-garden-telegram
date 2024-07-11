#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <IPAddress.h>
#include "App.h"
#include "env.h"

#define len(object) sizeof(object) / sizeof(*object)

const IPAddress hosts[] =
    {
        IPAddress(8, 8, 8, 8),
        IPAddress(8, 8, 4, 4)
    };

AppConfig conf {
    .WIFI_IP = IP_FROM_ENV(APP_ESP_IP),
    .WIFI_GATEWAY = IP_FROM_ENV(APP_WIFI_GATEWAY),
    .WIFI_SUBNET_MASK = IP_FROM_ENV(APP_WIFI_SUBNET_MASK),
    .WIFI_PRIMARY_DNS = IP_FROM_ENV(APP_WIFI_PRIMARY_DNS),
    .WIFI_SECONDARY_DNS = IP_FROM_ENV(APP_WIFI_SECONDARY_DNS),
    .WIFI_SSID = FROM_ENV(APP_WIFI_SSID),
    .WIFI_PASSWORD = FROM_ENV(APP_WIFI_PASSWORD),
    .BOT_TOKEN = FROM_ENV(APP_BOT_TOKEN),
    .ALLOWED_USER = FROM_ENV(APP_ALLOWED_USER),
    .PING_HOSTS = hosts,
    .PING_HOSTS_LENGTH = len(hosts),
    .WEB_SERVER_PORT = NUMBER_FROM_ENV(APP_WEB_SERVER_PORT),
    .LOCAL_TIMEZONE = "EET-2EEST,M3.5.0/3,M10.5.0/4",
    .ESP_LED_PIN = NUMBER_FROM_ENV(APP_ESP_LED_PIN),
    .MEGA_IO = Serial2
};
#endif