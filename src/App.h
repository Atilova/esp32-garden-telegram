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
#include "time.h"
#include "Doc.h"

#define repeat(n) for(int i = n; i--;)

struct tcp_pcb;  // Закрытие открытых неиспользуемых портов tcp.
                 // Это делать не ранее чем через 2 сек, после выключения web сервера
extern struct tcp_pcb* tcp_tw_pcbs;
extern "C" void tcp_abort(struct tcp_pcb* pcb);

void tcpCleanup(void)
    {
        while(tcp_tw_pcbs)
            tcp_abort(tcp_tw_pcbs);
    }

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
        const char* LOCAL_TIMEZONE;
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

            void transferToTelegramOrWeb(String buffer)
                {
                    if(!receiveBufferFromMega.available())
                        return;

                    receiveBufferFromMega.push(buffer);  // Добавляем строку в кольцевой буффер
                };

            void checkServiceCodeOrTransfer()
                {
                    if(receiveBufferFromMega.isEmpty())
                        return;

                    String message = receiveBufferFromMega.shift();

                    if(message == "2560ask?:inet")
                        return checkInternet();  // Возвратим меге inet.ok, если состояние CONNECTING_TO_TELEGRAM, иначе inet.no

                    if(message == "2560ask?:ntp")  // Синхронизация времени и дня недели для меги
                        return ntpSynchronization();

                    if(checkState(CONNECTING_TO_TELEGRAM))
                        bot->sendMessage(message);
                    else
                        webSourceEvents->send(message.c_str(), "newMegaMessage", millis());
                };

            void checkInternet()
                {
                    Serial2.println(checkState(CONNECTING_TO_TELEGRAM) ? "inet.ok" : "inet.no");
                };

            void ntpSynchronization()
                {
                    tm timeinfo;

                    configTime(0, 0, "pool.ntp.org");  // Забираем время из инета
                    setenv("TZ", localConf->LOCAL_TIMEZONE, 1);
                    tzset();

                    if(!getLocalTime(&timeinfo))
                        return transferToTelegramOrWeb("NTP:Ошибка синхронизации времени");

                    //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
                    Serial2.printf("time=%02d:%02d:%02d\r\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    Serial2.printf("day=%d\r\n", timeinfo.tm_wday);
                };

            boolean checkState(appState stateToCheck)
                {
                   return state == stateToCheck;  // Если state равен stateToCheck - вернем true, иначе false
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
                                            _app->transferToTelegramOrWeb(ttyData);
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
                                            _app->state = MAKE_PING;
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
                                            _app->updateMessageTelegram();
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
                                            // Есть подключение к телеграм и будем постоянно проверять Ping
                                            _app->checkInternet(); // Передаем inet.ok в мегу
                                            while(_app->pingWifi() && WiFi.isConnected())
                                                {
                                                    Serial.println("Ping. OK..");
                                                    _app->sleepTickTime(10000);  // Задержка между пинг, когда он прошел
                                                };

                                            Serial.println("Ping. or Wifi error");

                                            _app->state = WiFi.isConnected()
                                                ? PING_FAILED
                                                : CONNECTING_TO_WIFI;

                                            break;
                                        }
                                    case PING_FAILED:
                                        {
                                            Serial.println("2 case PING_FAILED:");
                                            // Во время работы телеграма отпал инет, поднимаем Webserver и ждем пинг ок
                                            _app->checkInternet();  // Передаем inet.no в мегу
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

            void handleAnyMessageFromTelegramOrWeb(String& message)
                {
                    if(message == "/start")
                        {
                            if(state == CONNECTING_TO_TELEGRAM)
                                bot->showMenuText("Keyboard loaded", "status \t options \t memory\n /help \t /help power");
                            return;
                        };

                    if(message == "/help")
                        {
                            transferToTelegramOrWeb(helpCommand[0]);
                            transferToTelegramOrWeb(helpCommand[1]);
                            transferToTelegramOrWeb(helpCommand[2]);
                            return transferToTelegramOrWeb(helpCommand[3]);
                        };

                    if(message == "/help power")
                        {
                            transferToTelegramOrWeb(helpCommand[4]);
                            return transferToTelegramOrWeb(helpCommand[5]);
                        };


                    if(message == "ntp")
                        {
                            if(state == CONNECTING_TO_TELEGRAM)
                                ntpSynchronization();
                            return;
                        };

                    if(message == "memory")
                        {
                            char buffer[64];
                            sprintf(buffer, " > Free memory ESP = %d", ESP.getFreeHeap());
                            return transferToTelegramOrWeb(buffer);
                        };

                    Serial2.println(message);  // Все остальное отдаем в мегу
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
                };

            void webServerHandleBodyRequest(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
                {
                    if(request->url() != "/send" || request->method() != HTTP_POST)
                        return request->send(400);

                    StaticJsonDocument<400> jsonBuffer;
                    DeserializationError error = deserializeJson(jsonBuffer, (const char*)data);
                    if(error)
                        return request->send(400);

                    char buffer[500];
                    serializeJson(jsonBuffer, buffer);
                    String msg = jsonBuffer["message"].as<String>();

                    webSourceEvents->send(buffer, "newUserMessage", millis());
                    handleAnyMessageFromTelegramOrWeb(msg);
                    return request->send(200);
                };

            void startWebServer()
                {
                    webServer->begin();
                    Serial.println("WebServer Started");
                    while(state == PING_FAILED)
                        {
                            checkServiceCodeOrTransfer();
                            sleepTickTime(100);
                        };
                    webSourceEvents->close();
                    webServer->end();
                    sleepTickTime(5000);  // Do not remove, wait for gracefull server end, before tcpCleanup()
                    tcpCleanup();
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
                                    sleepTickTime(5000);
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

             void updateMessageTelegram()
                {
                    Serial.println("Connected...");
                    bot->skipUpdates();  // Пропускаем старые сообщения
                    receiveBufferFromMega.clear();  // Очищаем кольцевой буфер
                    bot->sendMessage("🟢 Esp online");

                    while(state == CONNECTING_TO_TELEGRAM)
                        {
                            bot->tick();
                            checkServiceCodeOrTransfer();
                            sleepTickTime(30);
                        };
                };

            void receiveNewTelegramMessage(FB_msg& message)
                {
                    handleAnyMessageFromTelegramOrWeb(message.text);
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
                    bot->setTextMode(FB_TEXT);
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