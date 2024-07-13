#pragma once

#include <IPAddress.h>

#include "env.h"
#include "app/config.h"

#define len(object) sizeof(object) / sizeof(*object)

const IPAddress hosts[] =
    {
        IPAddress(8, 8, 8, 8),
        IPAddress(8, 8, 4, 4)
    };

AppConfig conf {
    .ESP_IP = IP_FROM_ENV(APP_ESP_IP),
    .ESP_LED_PIN = NUMBER_FROM_ENV(APP_ESP_LED_PIN),
    .ESP_TIMEZONE = "EET-2EEST,M3.5.0/3,M10.5.0/4",

    .WIFI_GATEWAY = IP_FROM_ENV(APP_WIFI_GATEWAY),
    .WIFI_SUBNET_MASK = IP_FROM_ENV(APP_WIFI_SUBNET_MASK),
    .WIFI_PRIMARY_DNS = IP_FROM_ENV(APP_WIFI_PRIMARY_DNS),
    .WIFI_SECONDARY_DNS = IP_FROM_ENV(APP_WIFI_SECONDARY_DNS),
    .WIFI_SSID = FROM_ENV(APP_WIFI_SSID),
    .WIFI_PASSWORD = FROM_ENV(APP_WIFI_PASSWORD),

    .MQTT_HOST = FROM_ENV(APP_MQTT_HOST),
    .MQTT_PORT = NUMBER_FROM_ENV(APP_MQTT_PORT),
    .MQTT_USERNAME = FROM_ENV(APP_MQTT_USERNAME),
    .MQTT_PASSWORD = FROM_ENV(APP_MQTT_PASSWORD),
    .MQTT_CLIENT_ID = FROM_ENV(APP_MQTT_CLIENT_ID),
    .MQTT_KEEP_ALIVE = NUMBER_FROM_ENV(APP_MQTT_KEEP_ALIVE),
    .MQTT_TIMEOUT = NUMBER_FROM_ENV(APP_MQTT_TIMEOUT),
    .MQTT_RECEIVE_TOPIC = FROM_ENV(APP_MQTT_RECEIVE_TOPIC),
    .MQTT_PUBLISH_TOPIC = FROM_ENV(APP_MQTT_PUBLISH_TOPIC),

    .WEB_SERVER_PORT = NUMBER_FROM_ENV(APP_WEB_SERVER_PORT),
    .WEB_SERVER_EVENT_SOURCE_PING_INTERVAL = NUMBER_FROM_ENV(APP_WEB_SERVER_EVENT_SOURCE_PING_INTERVAL),

    .PING_HOSTS = hosts,
    .PING_HOSTS_LENGTH = len(hosts),

    .MEGA_IO = Serial2
};