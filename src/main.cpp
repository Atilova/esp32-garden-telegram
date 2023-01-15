#include <Arduino.h>
#include "App.h"
#include "Config.h"
#include "Secret.h"

// Изменено в lib/FastBot/FastBot.h
// Original: void(*_callback)(FB_msg& msg)
// Now: #define SMART_CALLBACK std::function<void(FB_msg& msg)> _callback

#define RXD2 16
#define TXD2 17

App a(conf);

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); //порт обмена с  мегой

    a.run();
};


void loop() {

};