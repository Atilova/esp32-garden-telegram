#include <Arduino.h>
#include "App.h"
#include "Config.h"

// Изменено в lib/FastBot/FastBot.h
// Original: void(*_callback)(FB_msg& msg)
// Now: #define SMART_CALLBACK std::function<void(FB_msg& msg)> _callback


App application(conf);

void setup()
    {
        Serial.begin(115200);
        application.run();
    };

void loop()
    {

    };