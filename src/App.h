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
#include "SPIFFS.h"

#define repeat(n) for(int i = n; i--;)

// Закрытие открытых неиспользуемых соединений tcp.
// Это делать не ранее чем через 2 секунд, после выключения web сервера
struct tcp_pcb;
extern struct tcp_pcb* tcp_tw_pcbs;
extern "C" void tcp_abort(struct tcp_pcb* pcb);

void tcpCleanup(void)
    {
        while(tcp_tw_pcbs != NULL)
            tcp_abort(tcp_tw_pcbs);
    };

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
        uint8_t ESP_LED_PIN;
        HardwareSerial& MEGA_IO;
    };

enum appState
    {
        CONNECTING_TO_WIFI,
        INITIAL_PING,
        RUNNING_TELEGRAM,
        RUNNING_WEB_SERVER,
        DEVICE_REBOOT
    };

enum appSourceType
    {
        TELEGRAM_IO,
        WEB_SERVER_IO
    };

struct AppValues
    {
        char* tzEnvPointer = nullptr;
        boolean isInternetUp = false;
    };

class App
    {
        protected:
            constexpr static const uint16_t SOURCE_EVENT_PING_INTERVAL = 2000;
            constexpr static const char SYSTEM_MESSAGE_EVENT_TYPE[] = "newMegaMessage",
                                        USER_MESSAGE_EVENT_TYPE[] = "newUserMessage";

        private:
            TaskHandle_t xWifiTelegramAndWebHandle = NULL;
            TaskHandle_t xInternetPingHandle = NULL;
            TaskHandle_t xSerialMegaHandle = NULL;

            AppConfig* localConf;
            AppValues values;
            CircularBuffer<const char*, 15> receiveBufferFromMega;  // Will store pointers to char arrays on heap

            appState state = CONNECTING_TO_WIFI;
            appSourceType mainIO = TELEGRAM_IO;

            FastBot* bot;

            AsyncWebServer* webServer;
            AsyncEventSource *webSourceEvents;

            StaticJsonDocument<500> jsonBuffer;

            void addMessageToBuffer(const String& buffer)
                {
                    if(!receiveBufferFromMega.available())
                        return;

                    char *heapBuffer = new char[buffer.length()+1];  // Выделяем память на heap
                    strcpy(heapBuffer, buffer.c_str());
                    receiveBufferFromMega.push(heapBuffer);  // Добавляем строку в кольцевой буффер
                };

            void readBufferMessages()
                {
                    if(receiveBufferFromMega.isEmpty())
                        return;

                    const char* message = receiveBufferFromMega.shift();

                    if(!strcmp(message, "2560ask?:inet"))
                        pushInternetStatusToMega();
                    else if(!strcmp(message, "2560ask?:ntp"))
                        runNtpSynchronization();
                    else if(checkState(RUNNING_TELEGRAM))
                        bot->sendMessage(message);
                    else
                        webSourceEvents->send(message, SYSTEM_MESSAGE_EVENT_TYPE, millis());

                    delete [] message;  // Cleanup heap, освобождаем пямять
                };

            boolean updateInternetStatus(boolean updateAnyway=false)
                {
                    boolean result = pingWifi();
                    if(values.isInternetUp != result || updateAnyway)
                        {
                            values.isInternetUp = result;
                            pushInternetStatusToMega();
                        };
                    return values.isInternetUp;
                };

            void pushInternetStatusToMega()
                {
                    localConf->MEGA_IO.println(values.isInternetUp ? "inet.ok" : "inet.no");
                };

            void runNtpSynchronization()
                {
                    if(!values.isInternetUp)
                        return addMessageToBuffer("Couldn't obtain datetime, internet link down");

                    tm datetime;

                    configTime(0, 0, "pool.ntp.org");  // Забираем время из инета
                    strcpy(values.tzEnvPointer, localConf->LOCAL_TIMEZONE);
                    tzset();

                    if(!getLocalTime(&datetime))
                        return addMessageToBuffer("NTP:Ошибка синхронизации времени");

                    localConf->MEGA_IO.printf("time=%02d:%02d:%02d\r\n", datetime.tm_hour, datetime.tm_min, datetime.tm_sec);
                    localConf->MEGA_IO.printf("day=%d\r\n", datetime.tm_wday);
                };

            void getFreeHeapSize()
                {
                    char buffer[64];
                    sprintf(buffer, "ESP heap free size = %d", ESP.getFreeHeap());
                    addMessageToBuffer(buffer);
                };

            boolean checkState(appState stateToCheck)
                {
                   return state == stateToCheck;  // Если state равен stateToCheck - вернем true, иначе false
                };

            boolean checkIO(appSourceType ioToCheck)
                {
                    return mainIO == ioToCheck;
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
                            while(_app->localConf->MEGA_IO.available())  // Это Serial2
                                {
                                    char nextCharacter = _app->localConf->MEGA_IO.read();
                                    if(nextCharacter == SKIP_LINE)
                                        continue;

                                    if(nextCharacter == RESPONSE_END)
                                        {
                                            _app->addMessageToBuffer(ttyData);
                                            ttyData.clear();
                                            continue;
                                        };
                                    ttyData += nextCharacter;
                                };
                        };
                };

            static void primaryStateLoop(void* parameter)
                {
                    App* _app = static_cast<App*>(parameter);
                    for(;;)
                        {
                            switch(_app->state)
                                {
                                    case CONNECTING_TO_WIFI:
                                        {
                                            _app->connectToWifi();
                                            _app->state = INITIAL_PING;
                                            break;
                                        }
                                    case INITIAL_PING:
                                        {
                                            Serial.println("INITIAL PING");
                                            _app->updateInternetStatus(true);
                                            _app->state = _app->checkIO(TELEGRAM_IO)
                                                ? _app->values.isInternetUp
                                                    ? RUNNING_TELEGRAM
                                                    : WiFi.isConnected()
                                                        ? RUNNING_WEB_SERVER
                                                        : CONNECTING_TO_WIFI
                                                : RUNNING_WEB_SERVER;
                                            break;
                                        }
                                    case RUNNING_TELEGRAM:
                                        {
                                            _app->updateTelegram();
                                            break;
                                        }
                                    case RUNNING_WEB_SERVER:
                                        {
                                            _app->startWebServer();
                                            break;
                                        }
                                    case DEVICE_REBOOT:
                                        {
                                            ESP.restart();
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
                    App* _app = static_cast<App*>(parameter);
                    for(;;)
                        {
                            switch(_app->state)
                                {
                                    case RUNNING_TELEGRAM:
                                        {
                                            while(_app->checkIO(TELEGRAM_IO) && _app->updateInternetStatus() && WiFi.isConnected())
                                                _app->sleepTickTime(5000);

                                            _app->state = _app->checkIO(TELEGRAM_IO)
                                                ? WiFi.isConnected()
                                                    ? RUNNING_WEB_SERVER
                                                    : CONNECTING_TO_WIFI
                                                : RUNNING_WEB_SERVER;

                                            break;
                                        }
                                    case RUNNING_WEB_SERVER:
                                        {
                                            if(_app->checkIO(TELEGRAM_IO))
                                                {
                                                    while(_app->checkIO(TELEGRAM_IO) && !_app->updateInternetStatus() && WiFi.isConnected())
                                                        _app->sleepTickTime(4000);


                                                    _app->state = WiFi.isConnected()
                                                        ? _app->checkIO(TELEGRAM_IO)
                                                            ? RUNNING_TELEGRAM
                                                            : RUNNING_WEB_SERVER
                                                        : CONNECTING_TO_WIFI;

                                                    break;
                                                };

                                            while(_app->checkIO(WEB_SERVER_IO) && WiFi.isConnected())
                                                {
                                                    _app->updateInternetStatus();
                                                    _app->sleepTickTime(5000);
                                                };

                                            _app->state = WiFi.isConnected()
                                                ? INITIAL_PING
                                                : CONNECTING_TO_WIFI;

                                            break;
                                        }
                                    default:
                                        {
                                            _app->sleepTickTime(500);  //! DO NOT REMOVE, ESP CRASHES OTHERWISE
                                            break;
                                        };
                                };
                        };
                };

            void sendHelpPages(uint8_t startIndex, uint8_t endIndex)
                {
                    for(uint8_t index = startIndex; index <= endIndex; index++)
                        addMessageToBuffer(helpCommand[index]);
                };

            void handleAnyMessageFromTelegramOrWeb(String& message)
                {
                    if(message.equals("/help"))
                        return sendHelpPages(0, 3);

                    if(message.equals("/helppower"))
                        return sendHelpPages(4, 5);

                    if(message.equals("memory"))
                        return getFreeHeapSize();

                    if(message.equals("ntp"))
                        return runNtpSynchronization();

                    if(message.equals("reboot"))
                        return (void) (state = DEVICE_REBOOT);

                    localConf->MEGA_IO.println(message);  // Все остальное отдаем в мегу
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

                    webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
                        request->send(SPIFFS, "/index.html", "", false);
                    });

                    webServer->serveStatic("/static/", SPIFFS, "/static/");

                    webSourceEvents = new AsyncEventSource("/updates");
                    webServer->addHandler(webSourceEvents);

                    webSourceEvents->onConnect([](AsyncEventSourceClient *client) {
                        client->send(systemWebServerUpMessage, SYSTEM_MESSAGE_EVENT_TYPE, millis());
                    });
                };

            void webServerHandleBodyRequest(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
                {
                    if(request->url() != "/send" || request->method() != HTTP_POST)
                        return request->send(400);

                    DeserializationError error = deserializeJson(jsonBuffer, (const char*)data);
                    if(error)
                        return request->send(400);

                    char buffer[500];
                    serializeJson(jsonBuffer, buffer);  // strcpy(buffer, (const char*)data);
                    String message = jsonBuffer["message"].as<String>();

                    webSourceEvents->send(buffer, USER_MESSAGE_EVENT_TYPE, millis());

                    if(message.equals("/stopweb"))
                        {
                            mainIO = TELEGRAM_IO;
                            webSourceEvents->send(
                                values.isInternetUp
                                    ? systemWebServerDownMessage
                                    : systemWebServerUnableToEndMessage,
                                SYSTEM_MESSAGE_EVENT_TYPE,
                                millis()
                            );
                        }
                    else if(message.equals("/freezeweb"))
                        {
                            mainIO = WEB_SERVER_IO;
                            webSourceEvents->send(systemRemainWebServerMessage, SYSTEM_MESSAGE_EVENT_TYPE, millis());
                        }
                    else
                        handleAnyMessageFromTelegramOrWeb(message);
                    return request->send(200);
                };

            void startWebServer()
                {
                    webServer->begin();
                    Serial.println("WebServer Started");

                    uint32_t lastPingTime = millis() + SOURCE_EVENT_PING_INTERVAL;
                    while(checkState(RUNNING_WEB_SERVER))
                        {
                            if(lastPingTime < millis())
                                {
                                    webSourceEvents->send("", "ping", millis());
                                    lastPingTime = millis() + SOURCE_EVENT_PING_INTERVAL;
                                };

                            readBufferMessages();
                            sleepTickTime(100);
                        };

                    if(checkState(DEVICE_REBOOT))
                        webSourceEvents->send(systemDeviceRebootMessage, SYSTEM_MESSAGE_EVENT_TYPE, millis());
                    else if(checkState(DEVICE_REBOOT))
                        webSourceEvents->send(systemEndWebServerStartTelegramMessage, SYSTEM_MESSAGE_EVENT_TYPE, millis());

                    webSourceEvents->close();
                    webServer->end();
                    sleepTickTime(7000);  //! Do not remove, wait for graceful server end, before tcpCleanup()
                    tcpCleanup();
                    Serial.println("CleanUP finished");
                };

            void resetWifi()
                {
                    WiFi.disconnect();
                    WiFi.softAPdisconnect();
                    WiFi.mode(WIFI_OFF);
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

            boolean pingWifi()
                {
                    for(uint8_t index=0; index < localConf->PING_HOSTS_LENGTH; index++)
                        {
                            IPAddress pingableHost = localConf->PING_HOSTS[index];
                            if(Ping.ping(pingableHost, 2))  // Сделаем две попытки пинга, если он не проходит
                                return true;
                        };
                    return false;
                };

             void updateTelegram()
                {
                    Serial.println("Connected...");
                    bot->skipUpdates();  // Пропускаем старые сообщения
                    receiveBufferFromMega.clear();  // Очищаем кольцевой буфер
                    bot->sendMessage(systemTelegramUpMessage);
                    while(checkState(RUNNING_TELEGRAM))
                        {
                            bot->tick();
                            readBufferMessages();  // Если что-то есть в кольцевом буфере, выводим в телеграм
                            sleepTickTime(30);
                        };

                    if(checkState(DEVICE_REBOOT))
                        bot->sendMessage(systemDeviceRebootMessage);
                };

            void receiveNewTelegramMessage(FB_msg& message)
                {
                    if(message.text == "/start")
                        return (void) bot->showMenuText("Keyboard loaded", telegramVirtualKeyboard);

                    if(message.text.equals("/startweb"))
                        return (void) ({
                            char buffer[30];
                            sprintf(buffer, "http://%s", WiFi.localIP().toString());
                            bot->inlineMenuCallback(systemTelegramDownMessage, "Open App", buffer);
                            mainIO = WEB_SERVER_IO;
                        });

                    handleAnyMessageFromTelegramOrWeb(message.text);  // Получаем сообщение и телеграма
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
                    bot->setTextMode(FB_HTML);
                };

            void setupTimezone()
                {
                    unsetenv("TZ");
                    setenv("TZ", localConf->LOCAL_TIMEZONE, 1);
                    tzset();
                    values.tzEnvPointer = getenv("TZ");  // Instead of using setenv - (memory leak), use this pointer and strcpy
                };

        public:
            App(AppConfig& data)
                {
                    localConf = &data;
                    localConf->MEGA_IO.begin(115200);   // Это Serial2.begin(115200)
                    SPIFFS.begin();
                    pinMode(localConf->ESP_LED_PIN, OUTPUT);
                    setupTelegram();
                    setupWebServer();
                    setupTimezone();
                };

            ~App()
                {
                    delete bot;
                    delete webServer;
                    delete webSourceEvents;
                    delete values.tzEnvPointer;

                    while(!receiveBufferFromMega.isEmpty())
                        delete [] receiveBufferFromMega.pop();
                };

            void run()
                {
                    xTaskCreate(readSerialMega, "readSerialMega", 8000, static_cast<void*>(this), 1, &xSerialMegaHandle);
                    xTaskCreate(primaryStateLoop, "primaryStateLoop", 12000, static_cast<void*>(this), 1, &xWifiTelegramAndWebHandle);
                    xTaskCreate(secondaryStateLoop, "secondaryStateLoop", 16000, static_cast<void*>(this), 1, &xInternetPingHandle);
                };
    };
#endif