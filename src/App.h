#ifndef _APP_H_
#define _APP_H_

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <IPAddress.h>
#include <FastBot.h>
#include <CircularBuffer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncEventSource.h>
#include <ArduinoJson.h>

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
        uint32_t WEB_SERVER_PORT;
        const char* (*WEB_BUTTONS)[2];
        uint8_t WEB_BUTTONS_LENGTH;
        uint8_t ESP_LED_PIN;  // LED ESP32 вывод - 2
    };

enum appState
    {
        CONNECTING_TO_WIFI,
        MAKE_PING,
        CONNECTING_TO_TELEGRAM,
        PING_FAILED,
        TEST
    };


class App
    {
        private:
            TaskHandle_t xWifiPingBotHandle = NULL;
            TaskHandle_t xPingWebHandle = NULL;
            TaskHandle_t xSerialMegaHandle = NULL;

            AppConfig* localConf;
            appState state = CONNECTING_TO_WIFI;
            CircularBuffer<String, 10> receiveBufferFromMega;

            FastBot* bot;

            AsyncWebServer* webServer;
            AsyncEventSource *webSourceEvents;

            void transferToTelegram(String buffer)
                {
                    if(!receiveBufferFromMega.available())
                        return;

                    receiveBufferFromMega.push(buffer);  // Добавляем строку в кольцевой буффер
                };

            void sleepTickTime(uint16_t delayMs)
                {
                    vTaskDelay(delayMs / portTICK_RATE_MS);
                };

            static void readSerialMega(void* parameter)
                {
                    constexpr static const char RESPONSE_END = '%',
                                                SKIP_LINE = '\r';
                    String ttyData;
                    App* _app = static_cast<App*>(parameter);

                    for(;;)
                        {
                            while(Serial2.available())
                                {
                                    char nextCharacter = Serial2.read();
                                    if(nextCharacter == SKIP_LINE)
                                        continue;

                                    if(nextCharacter == RESPONSE_END)
                                        {
                                            _app->transferToTelegram(ttyData);
                                            ttyData.clear();
                                            continue;
                                        };
                                    ttyData += nextCharacter;
                                };
                        };
                };

            static void primaryStateLoop(void* parameter)  // Основной цикл проверки wifi, ping
                {
                    App* _app = static_cast<App*>(parameter);
                    for(;;)
                        {
                            switch(_app->state)
                                {
                                    case CONNECTING_TO_WIFI:
                                        {
                                            _app->connectToWifi();
                                            // _app->state = MAKE_PING;
                                            _app->state = TEST;
                                            break;
                                        }
                                    case MAKE_PING:
                                        {
                                            _app->state = _app->pingWifi()  // Одноразовый пинг для подкл. к телеграм
                                                     ? CONNECTING_TO_TELEGRAM
                                                     : WiFi.isConnected()
                                                        ? PING_FAILED
                                                        : CONNECTING_TO_WIFI;

                                            break;
                                        }
                                    case CONNECTING_TO_TELEGRAM:
                                        {
                                            _app->readTelegram();
                                            break;
                                        }
                                    case PING_FAILED:
                                        {
                                            while(!_app->pingWifi() && WiFi.isConnected())
                                                _app->sleepTickTime(4000);  // Задержка между пинг, когда он не прошел

                                            _app->state = WiFi.isConnected()
                                                ? CONNECTING_TO_TELEGRAM
                                                : CONNECTING_TO_WIFI;
                                            break;
                                        }
                                    default:
                                        {
                                            _app->sleepTickTime(500);
                                            break;
                                        };
                                };
                        };

                };

            static void secondaryStateLoop(void* parameter)
                {
                    Serial.println("secondaryStateLoop");

                    App* _app = static_cast<App*>(parameter);
                    for(;;)
                        {
                            switch(_app->state)
                                {
                                    case CONNECTING_TO_TELEGRAM:
                                        {
                                            Serial.println("2 case CONNECTING_TO_TELEGRAM");

                                            // Есть подключение к телеграм и будем постоянно проверять Ping
                                            while(_app->pingWifi() && WiFi.isConnected())
                                                {
                                                    Serial.println("Ping. OK..");
                                                    _app->sleepTickTime(7000);  // Задержка между пинг, когда он прошел
                                                };

                                            Serial.println("Ping. or Wifi error");

                                            _app->state = WiFi.isConnected()
                                                ? PING_FAILED
                                                : CONNECTING_TO_WIFI;

                                            break;
                                        }
                                    case TEST: //PING_FAILED:
                                        {
                                            Serial.println("2 case PING_FAILED:");
                                            // во время работы телеграма отпал инет, поднимаем Webserver и ждем пинг ок
                                            _app->startWebServer();
                                            break;
                                        }
                                    default:
                                        {
                                            _app->sleepTickTime(500);  // Категорически не убирать задержку!!!!не будет работать!!!
                                            break;
                                        };
                                };
                        };
                };

            void setupWebServer()
                {
                    webServer = new AsyncWebServer(localConf->WEB_SERVER_PORT);

                    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

                    webServer->onRequestBody(std::bind(
                        &App::webServerHandleBodyRequest,
                        this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3,
                        std::placeholders::_4,
                        std::placeholders::_5
                    ));

                    webServer->onNotFound([](AsyncWebServerRequest *request) {
                        request->send(404, "text/html", "<script>location.replace(\"/\");</script>");
                    });

                    webSourceEvents = new AsyncEventSource("/updates");
                    webServer->addHandler(webSourceEvents);
                    webSourceEvents->onConnect(std::bind(
                        &App::sourceEventOnConnect,
                        this,
                        std::placeholders::_1
                    ));
                };

            void sourceEventOnConnect(AsyncEventSourceClient *client) {
                Serial.println("New event");
            };

            void webServerHandleBodyRequest(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
                {
                    if(request->url() != "/send" || request->method() != HTTP_POST)
                        return request->send(400);

                    StaticJsonDocument<400> jsonBuffer;
                    DeserializationError error = deserializeJson(jsonBuffer, (const char*)data);
                    if(error)
                        return request->send(400);


                    char buffer[200];
                    serializeJson(jsonBuffer, buffer);
                    const char* msg = jsonBuffer["message"];
                    Serial.println(msg);
                    Serial2.println(msg);
                    webSourceEvents->send(buffer, "newUserMessage", millis());
                    return request->send(200);
                };

            void startWebServer()
                {
                    webServer->begin();
                    Serial.println("ready");
                    while(state == TEST) {
                        if(!receiveBufferFromMega.isEmpty())
                            webSourceEvents->send(receiveBufferFromMega.shift().c_str(), "newMegaMessage", millis());

                        sleepTickTime(100);
                    };
                    webServer->end();
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
                    digitalWrite(localConf->ESP_LED_PIN, LOW);
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
                    digitalWrite(localConf->ESP_LED_PIN, HIGH);
                    Serial.println("wifi connected");
                };

            bool pingWifi()
                {
                    for(uint8_t index=0; index < localConf->PING_HOSTS_LENGTH; index++)
                      {
                        IPAddress pingableHost = localConf->PING_HOSTS[index];
                        if(Ping.ping(pingableHost, 2))  // Сделаем две попытки пинга, если он не проходит
                          return true;
                      };
                    return false;
                };

             void readTelegram()
                {
                    Serial.println("Connected...");
                    bot->skipUpdates();  // Пропускаем старые сообщения
                    receiveBufferFromMega.clear();  // Очищаем кольцевой буфер

                    bot->showMenu("status \t options \n /help");

                    while(state == CONNECTING_TO_TELEGRAM)
                        {
                            bot->tick();
                            if(!receiveBufferFromMega.isEmpty())
                                bot->sendMessage(receiveBufferFromMega.shift());  // Первый элемент буфера читаем и удаляем
                            sleepTickTime(100);
                        };
                };

            void receiveNewTelegramMessage(FB_msg& message)
                {
                    bot->sendMessage(message.text);  // Echo

                    Serial.println(message.text);
                    Serial2.println(message.text);
                };

            void setupTelegram()
                {
                    bot = new FastBot(localConf->BOT_TOKEN);  // Создаем указатель на heap при работе esp
                    bot->setChatID(localConf->ALLOWED_USER);
                    bot->setPeriod(500);
                    bot->attach(std::bind(
                        &App::receiveNewTelegramMessage,
                        this,
                        std::placeholders::_1
                    ));
                };

        public:
            App(AppConfig& data)
                {
                    localConf = &data;
                    pinMode(localConf->ESP_LED_PIN, OUTPUT);
                    setupTelegram();
                    setupWebServer();
                };

            ~App()
                {
                    delete bot;
                    delete webServer;
                    delete webSourceEvents;
                };

            void run()
                {
                    xTaskCreate(readSerialMega, "readSerialMega", 8000, static_cast<void*>(this), 1, &xSerialMegaHandle);
                    xTaskCreate(primaryStateLoop, "primaryStateLoop", 8000, static_cast<void*>(this), 1, &xWifiPingBotHandle);
                    xTaskCreate(secondaryStateLoop, "secondaryStateLoop", 16000, static_cast<void*>(this), 1, &xPingWebHandle);
                };
    };
#endif