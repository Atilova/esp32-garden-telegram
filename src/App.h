#ifndef _APP_H_
#define _APP_H_

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <IPAddress.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <CircularBuffer.h>

#define repeat(n) for(int i = n; i--;)


struct AppConfig {
    IPAddress WIFI_IP;
    IPAddress WIFI_GATEWAY;
    IPAddress WIFI_SUBNET_MASK;
    IPAddress WIFI_PRIMARY_DNS;
    IPAddress WIFI_SECONDARY_DNS;
    const char* WIFI_SSID;
    const char* WIFI_PASSWORD;
    const char* BOT_TOKEN;
    const char* ALLOWED_USER;
    const IPAddress* PING_HOSTS;
    uint8_t PING_HOSTS_LENGTH;
    uint8_t LED_PIN_ESP;  //LED ESP32 выв- 2  для платы с внешним светодиодом
};

enum appState { 
    CONNECTING_TO_WIFI, 
    MAKE_PING, 
    CONNECTING_TO_TELEGRAM,
    PING_FAILED
};

class App 
    {
        private:
            TaskHandle_t xWifiPingBotHandle = NULL;
            TaskHandle_t xPingWebHandle = NULL;
            TaskHandle_t xReceiveBufferHandle = NULL;

            appState state = CONNECTING_TO_WIFI;
            AppConfig* localConf;

            WiFiClientSecure client;
            UniversalTelegramBot* bot;
            CircularBuffer<String, 10> receiveBufferFromMega;

        public:
            void transferToTelegram(String buffer)
                {
                    if(receiveBufferFromMega.isFull() || state != CONNECTING_TO_TELEGRAM)
                        return;
                    
                    receiveBufferFromMega.push(buffer); //добавляем строку в кольцевой буффер
                }

            void sleepTickTime(uint16_t delayMs)
                {
                    vTaskDelay(delayMs / portTICK_RATE_MS);
                }

            static void primaryStateLoop(void* pv) //основной цикл проверки wifi, ping
                {
                    App* ekz = static_cast<App*>(pv);
                    for(;;)
                        {
                            switch(ekz->state)
                                {
                                    case CONNECTING_TO_WIFI:
                                        {
                                            ekz->connectToWifi();             
                                            break;
                                        } 
                                    case MAKE_PING:
                                        {
                                            ekz->state = ekz->pingWifi() //одноразовый пинг для подкл. к телеграм
                                                     ? CONNECTING_TO_TELEGRAM 
                                                     : WiFi.isConnected() 
                                                        ? PING_FAILED 
                                                        : CONNECTING_TO_WIFI;
                                            
                                            break;
                                        }
                                    case CONNECTING_TO_TELEGRAM:
                                        {   
                                            //get _updates тут
                                            ekz->readTelegram();
                                            break;
                                        }
                                    case PING_FAILED:
                                        {
                                            while(!ekz->pingWifi() && WiFi.isConnected())
                                                ekz->sleepTickTime(4000); //задержка между пинг, когда он не прошел

                                            ekz->state = WiFi.isConnected() 
                                                ? CONNECTING_TO_TELEGRAM 
                                                : CONNECTING_TO_WIFI;
                                            break;
                                        }
                                    default:
                                        break;
                                }
                        }

                };

            static void secondaryStateLoop(void* pv) //
                {
                    Serial.println("secondaryStateLoop");
                    
                    App* ekz = static_cast<App*>(pv);
                    for(;;)
                        {  
                            switch(ekz->state)
                                {
                                    case CONNECTING_TO_TELEGRAM:
                                        { 
                                            Serial.println("2 case CONNECTING_TO_TELEGRAM");
                                            
                                            // есть подключение к телеграм и будем постоянно проверять Ping 
                                            while(ekz->pingWifi() && WiFi.isConnected()) {
                                                Serial.println("Ping. OK..");
                                                ekz->sleepTickTime(31000); //задержка между пинг, когда он не прошел
                                            }

                                            Serial.println("Ping. or Wifi error");

                                            ekz->state = WiFi.isConnected() 
                                                ? PING_FAILED 
                                                : CONNECTING_TO_WIFI;

                                            break;
                                        }
                                    case PING_FAILED:
                                        {
                                            Serial.println("2 case PING_FAILED:");
                                            // во время работы телеграма отпал инет, поднимаем Webserver и ждем пинг ок
                                            //attach
                                            while(ekz->state == PING_FAILED) {
                                                // tics
                                            }

                                            // detach 
                                            break;
                                        }
                                    default:
                                        {
                                            //Serial.print("State -> ");
                                            //Serial.println(ekz->state);
                                            ekz->sleepTickTime(500);
                                            break;
                                        }
                                }
                        }
                        Serial.println("IN the end...");

                };
        
        
            void resetWifi()
                {
                    WiFi.disconnect(); // обрываем WIFI соединения
                    WiFi.softAPdisconnect(); // отключаем точку доступа, т.е режим роутера
                    WiFi.mode(WIFI_OFF); // отключаем WIFI
                    sleepTickTime(500); //задержка 0,5 сек
                }
            
            void connectToWifi()
                {
                    while(!WiFi.isConnected()) //пока не подключено к вифи
                        {
                            resetWifi(); 
                            repeat(3) 
                                {
                                    if(WiFi.isConnected()) //если подключились - вываливаемся
                                        break; 

                                    WiFi.mode(WIFI_STA); // режим клиента
                                    WiFi.config(
                                        localConf->WIFI_IP, 
                                        localConf->WIFI_GATEWAY, 
                                        localConf->WIFI_SUBNET_MASK, 
                                        localConf->WIFI_PRIMARY_DNS
                                    );
                                    WiFi.begin(localConf->WIFI_SSID, localConf->WIFI_PASSWORD); 
                                    sleepTickTime(1000);                      
                                }
                        }
                    state = MAKE_PING; 
                    digitalWrite(localConf->LED_PIN_ESP, HIGH); 
                    Serial.println("wifi connected");
                }

            bool pingWifi()
                {
                    for(uint8_t index=0; index < localConf->PING_HOSTS_LENGTH; index++) 
                      {              
                        IPAddress pingableHost =  localConf->PING_HOSTS[index];
                        if(Ping.ping(pingableHost)) 
                          return true;
                      }
                    return false;  
                }

            
             void readTelegram()
                {
                    Serial.println("Connected...");
                    while(bot->getUpdates(bot->last_message_received + 1)) {} //вычитываем старые сообщения
                    receiveBufferFromMega.clear();

                    while(state == CONNECTING_TO_TELEGRAM) 
                        {
                            int newMessages = bot->getUpdates(bot->last_message_received + 1);
                            while(newMessages)
                                {
                                String text = bot->messages[0].text;
                                // Serial.printf("Received -> %s\n", text);
                                newMessages = bot->getUpdates(bot->last_message_received + 1);
                                Serial2.println(text); //передаем в мегу , что есть инет
                                bot->sendMessage(localConf->ALLOWED_USER,  text, "html"); //мой id - "504052635"
                                }

                                // if(newMessages)
                                //     {
                                //         String text = bot->messages[0].text;
                                //         Serial.printf("Received -> %s\n", text);
                                //         bot->sendMessage(localConf->ALLOWED_USER,  text, "html"); //мой id - "504052635"
                                //     }
                                // }
                            
                            if(!receiveBufferFromMega.isEmpty() && state == CONNECTING_TO_TELEGRAM) {
                                //Serial.println("IN");
                               // String text = receiveBufferFromMega.shift();
                                //Serial.printf("sending -> %s\n", text);
                                bot->sendMessage(localConf->ALLOWED_USER, receiveBufferFromMega.shift());  //первый элемент буфера читаем и удаляем
                            }

                            sleepTickTime(800);
                        }
                }

        public:
            App(AppConfig& data) 
                {
                    localConf = &data;
                    bot = new UniversalTelegramBot(localConf->BOT_TOKEN, client);
                    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); //с 22 июня 2021, без этого с esp32 не работает
                    pinMode(data.LED_PIN_ESP, OUTPUT);
                }; 
            
            ~App() {};

            void run()
                {
                    
                    
                    
                    xTaskCreate(secondaryStateLoop, "secondaryStateLoop", 8000, static_cast<void*>(this), 1, &xPingWebHandle);
                    xTaskCreate(primaryStateLoop, "primaryStateLoop", 8000, static_cast<void*>(this), 1, &xWifiPingBotHandle);
                    
                    // xTaskCreatePinnedToCore(secondaryStateLoop, "secondaryStateLoop", 8000, static_cast<void*>(this), 1, &xPingWebHandle, 1);
                    // xTaskCreatePinnedToCore(primaryStateLoop, "primaryStateLoop", 8000, static_cast<void*>(this), 1, &xWifiPingBotHandle, 0);
                }

            void tester() {
                primaryStateLoop(static_cast<void*>(this));
            }

    };


#endif