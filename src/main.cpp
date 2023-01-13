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
String ttyData;

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); //порт обмена с  мегой

    a.run();
};

void readMegaSerial()
  {
        constexpr static const byte RESPONSE_END = '%',
                                    SKIP_LINE = '\r';

        while(Serial2.available())
            {
                char nextCharacter = Serial2.read();
                //delay (1);

                // Если перевод в начало внутри текста, пропускаем его, чтобы не было еще одной пустой строки
                if(nextCharacter == SKIP_LINE)
                    continue;

                if(nextCharacter == RESPONSE_END)
                    {
                        Serial.println("Встретился %, выводим в телегу");
                        a.transferToTelegram(ttyData);
                        ttyData.clear();
                    };


                ttyData += nextCharacter;
            };
    };

void loop() {
  readMegaSerial();
};