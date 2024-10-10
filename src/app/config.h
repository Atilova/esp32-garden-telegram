#pragma once

#include <IPAddress.h>

struct AppConfig
    {
		IPAddress ESP_IP;
        uint8_t ESP_LED_PIN;
        const char* ESP_TIMEZONE;

        IPAddress WIFI_GATEWAY;
        IPAddress WIFI_SUBNET_MASK;
        IPAddress WIFI_PRIMARY_DNS;
        IPAddress WIFI_SECONDARY_DNS;
        const char* WIFI_SSID;
        const char* WIFI_PASSWORD;

        const char* MQTT_HOST;
        const uint32_t MQTT_PORT;
        const char* MQTT_USERNAME;
        const char* MQTT_PASSWORD;
        const char* MQTT_CLIENT_ID;
        const uint16_t MQTT_KEEP_ALIVE;
        const uint16_t MQTT_TIMEOUT;

        const char* MQTT_RECEIVE_TOPIC;
        const char* MQTT_PUBLISH_DEFAULT_TOPIC;
        const char* MQTT_PUBLISH_EVENTS_TOPIC;
        const char* MQTT_PUBLISH_RESPONSE_TOPIC;

        uint32_t WEB_SERVER_PORT;
        uint16_t WEB_SERVER_EVENT_SOURCE_PING_INTERVAL;

        const IPAddress* PING_HOSTS;
        uint8_t PING_HOSTS_LENGTH;

        HardwareSerial& MEGA_IO;
        uint8_t RX_PIN;
        uint8_t TX_PIN; 
    };
