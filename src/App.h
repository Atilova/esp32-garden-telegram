#ifndef _APP_H_
#define _APP_H_

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <IPAddress.h>
#include <FastBot.h>
#include <CircularBuffer.h>

#define repeat(n) for(int i = n; i--;)


struct AppConfig
    {
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
        uint8_t LED_PIN_ESP;  // LED ESP32 вывод - 2
    };

enum appState
    {
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

            AppConfig* localConf;
            appState state = CONNECTING_TO_WIFI;

            FastBot* bot;
            CircularBuffer<String, 10> receiveBufferFromMega;

        public:
            void transferToTelegram(String buffer)
                {
                    if(!receiveBufferFromMega.available() || state != CONNECTING_TO_TELEGRAM)
                        return;

                    receiveBufferFromMega.push(buffer);  // Добавляем строку в кольцевой буффер
                };

        private:
            void sleepTickTime(uint16_t delayMs)
                {
                    vTaskDelay(delayMs / portTICK_RATE_MS);
                };

            static void primaryStateLoop(void* parameter)  // Основной цикл проверки wifi, ping
                {
                    App* ekz = static_cast<App*>(parameter);
                    for(;;)
                        {
                            switch(ekz->state)
                                {
                                    case CONNECTING_TO_WIFI:
                                        {
                                            digitalWrite(ekz->localConf->LED_PIN_ESP, LOW);
                                            ekz->connectToWifi();
                                            break;
                                        }
                                    case MAKE_PING:
                                        {
                                            ekz->state = ekz->pingWifi()  // Одноразовый пинг для подкл. к телеграм
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
                                                ekz->sleepTickTime(4000);  // Задержка между пинг, когда он не прошел

                                            ekz->state = WiFi.isConnected()
                                                ? CONNECTING_TO_TELEGRAM
                                                : CONNECTING_TO_WIFI;
                                            break;
                                        }
                                    default:
                                        break;
                                };
                        };

                };

            static void secondaryStateLoop(void* parameter)
                {
                    Serial.println("secondaryStateLoop");

                    App* ekz = static_cast<App*>(parameter);
                    for(;;)
                        {
                            switch(ekz->state)
                                {
                                    case CONNECTING_TO_TELEGRAM:
                                        {
                                            Serial.println("2 case CONNECTING_TO_TELEGRAM");

                                            // Есть подключение к телеграм и будем постоянно проверять Ping
                                            while(ekz->pingWifi() && WiFi.isConnected())
                                                {
                                                    Serial.println("Ping. OK..");
                                                    ekz->sleepTickTime(7000);  // Задержка между пинг, когда он не прошел
                                                };

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
                                            ekz->sleepTickTime(500);
                                            break;
                                        };
                                };
                        };
                };

            void resetWifi()
                {
                    WiFi.disconnect();  // Обрываем WIFI соединения
                    WiFi.softAPdisconnect();  // Отключаем точку доступа, т.е режим роутера
                    WiFi.mode(WIFI_OFF);  // Отключаем WIFI
                    sleepTickTime(500);
                };

            void connectToWifi()
                {
                    while(!WiFi.isConnected())  // Пока не подключено к вифи
                        {
                            resetWifi();
                            repeat(3)
                                {
                                    if(WiFi.isConnected())  // Если подключились - вываливаемся
                                        break;

                                    WiFi.mode(WIFI_STA);  // Режим клиента
                                    WiFi.config(
                                        localConf->WIFI_IP,
                                        localConf->WIFI_GATEWAY,
                                        localConf->WIFI_SUBNET_MASK,
                                        localConf->WIFI_PRIMARY_DNS
                                    );
                                    WiFi.begin(localConf->WIFI_SSID, localConf->WIFI_PASSWORD);
                                    sleepTickTime(1000);
                                };
                        };
                    state = MAKE_PING;
                    digitalWrite(localConf->LED_PIN_ESP, HIGH);
                    Serial.println("wifi connected");
                }

            bool pingWifi()
                {
                    for(uint8_t index=0; index < localConf->PING_HOSTS_LENGTH; index++)
                      {
                        IPAddress pingableHost = localConf->PING_HOSTS[index];
                        if(Ping.ping(pingableHost))
                          return true;
                      }
                    return false;
                };

             void readTelegram()
                {
                    Serial.println("Connected...");
                    bot->skipUpdates();  // Вычитываем старые сообщения
                    receiveBufferFromMega.clear();

                    bot->showMenu("status \t options \n /help");

                    while(state == CONNECTING_TO_TELEGRAM)
                        {
                            bot->tick();
                            if(!receiveBufferFromMega.isEmpty() && state == CONNECTING_TO_TELEGRAM)
                                bot->sendMessage(receiveBufferFromMega.shift());  // Первый элемент буфера читаем и удаляем
                            sleepTickTime(80);
                        };
                };

            void receiveNewTelegramMessage(FB_msg& message)
                {
                    bot->sendMessage(message.text);  // Echo

                    Serial.println(message.text);
                    Serial2.println(message.text);
                };

        public:
            App(AppConfig& data)
                {
                    localConf = &data;
                    pinMode(localConf->LED_PIN_ESP, OUTPUT);

                    bot = new FastBot(localConf->BOT_TOKEN);  // Создаем указатель на heap при работе esp
                    bot->setChatID(localConf->ALLOWED_USER);
                    bot->setPeriod(500);

                    bot->attach(std::bind(
                        &App::receiveNewTelegramMessage,
                        this,
                        std::placeholders::_1
                    ));
                };

            ~App()
                {
                    delete bot;
                };

            void run()
                {
                    xTaskCreate(secondaryStateLoop, "secondaryStateLoop", 8000, static_cast<void*>(this), 1, &xPingWebHandle);
                    xTaskCreate(primaryStateLoop, "primaryStateLoop", 8000, static_cast<void*>(this), 1, &xWifiPingBotHandle);
                };
    };
#endif