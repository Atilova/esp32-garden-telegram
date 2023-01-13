#include <Arduino.h>
#include "App.h"
#include "Config.h"
// #include <UniversalTelegramBot.h>

// UniversalTelegramBot bot(BOT_TOKEN, client);

//Serial2 ESP32 порт в мегу 
#define RXD2 16    // 
#define TXD2 17    // 

App a(conf);
String ttydata = "";

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); //порт обмена с  мегой
  
  
   a.run();
}

void loop() {
  // while(Serial2.available()) 
  //   {
  //     // Serial.println("Available");
  //     String r = Serial2.readStringUntil('%');
  //     Serial.printf("Received from mega -> %s\n", r);
  //     a.transferToTelegram(r);

  //   }



    while ( Serial2.available() > 0)
      { //Serial.println("Есть что принимать, принпроверяем наличие новых сообщений от меняимаем");
        char c = Serial2.read();
        //delay (1);

        //разбор принятого символа
        if (c == '\r') //если перевод в начало внутри текста, пропускаем его, чтобы не было еще одной пустой строки
        { }  // пропускаем вхождящий символ
        else 
          if (c == '%') //если встретился символ конца выводимого буфера, отдаем весь буфер в Телеграм
            {   Serial.println ("Встретился %, выводим в телегу");
              // ttydata += c;
              //flag_buf = 1; //              
              
              if(!ttydata.isEmpty())
                a.transferToTelegram(ttydata);
              ttydata = "";
            }
            else //иначе добавляем этот символ в стринг
              ttydata += c;

       }// while


}

