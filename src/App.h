#ifndef _APP_H_
#define _APP_H_

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <IPAddress.h>
#include <FastBot.h>
#include <CircularBuffer.h>
#include <GyverPortal.h>

#define repeat(n) for(int i = n; i--;)


struct PortalOptions
    {
        bool needToCleanCommandInput = false;
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

            FastBot* bot;
            CircularBuffer<String, 10> receiveBufferFromMega;

            GyverPortal ui;
            PortalOptions portalOptions;

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
                                        }
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
                                            _app->sleepTickTime(500); //категорически не убирать задержку!!!!не будет работать!!!
                                            break;
                                        };
                                };
                        };
                };

            void buildWebPage()
                {
                    GP.BUILD_BEGIN(1010);
                    GP.THEME(GP_DARK);
                    GP.PAGE_TITLE("Garden");

                    GP.UPDATE("txt1");
                    
                    // GP.HR();

                    
                    GP.BUTTON_MINI("btn2", "status");
                    GP.BUTTON_MINI("btn3", "info");
                    GP.BUTTON_MINI("btn4", "list");
                    GP.BUTTON_MINI("btn5", "on");
                    GP.BUTTON_MINI("btn6", "shutdown");
                    GP.BUTTON_MINI("btn7", "stop");
                    GP.BUTTON_MINI("btn8", "tank");
                    GP.BUTTON_MINI("btn9", "active");
                    GP.BUTTON_MINI("btn10", ".deep water control");
                    GP.BUTTON_MINI("btn11", "options");
                    GP.BUTTON_MINI("btn12", "/help");
                    GP.BREAK();
                    // GP.HR();

                    GP.TEXT("txt1", "","","83%");
                    GP.BUTTON_MINI("btn1", "Send", "txt1");
                    GP.BREAK();
                    GP.AREA_LOG(30, 1000, "90%");
                    GP.BREAK();

                    GP.BUILD_END();
                }

            void action() 
                {
                    if(ui.update() && portalOptions.needToCleanCommandInput) 
                        {
                            String emptyInput = "\r\n";
                            portalOptions.needToCleanCommandInput = false;
                            ui.updateString("txt1", emptyInput);
                        };

                    
                    


                    if(ui.click("btn1")) 
                        {
                            Serial.print("IN btn1 -> ");
                            // отправляем обратно в "монитор" содержимое поля, оно пришло по клику кнопки
                            
                            String msg = ui.getString("btn1");
                            Serial2.println(msg);
                            ui.log.println(msg);                             
                            portalOptions.needToCleanCommandInput = true;                            
                        };
                };

            void startWebServer()
                {
                    Serial.println("Starting WebServer...");
                    ui.start();
                    ui.log.start(5000);
                                        
                    while(state == TEST)
                        {
                            ui.tick();
                            if(!receiveBufferFromMega.isEmpty())
                                {
                                    ui.log.println("_________________________________________");
                                    ui.log.println(receiveBufferFromMega.shift());
                                }
                            sleepTickTime(90);
                        };

                    ui.stop();
                    ui.log.stop();
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
                        if(Ping.ping(pingableHost, 2)) //сделаем две попытки пинга, если он не проходит
                          return true;
                      }
                    return false;
                };

             void readTelegram()
                {
                    Serial.println("Connected...");
                    bot->skipUpdates();  // пропускаем старые сообщения
                    receiveBufferFromMega.clear(); // очищаем кольцевой буфер

                    bot->showMenu("status \t options \n /help");

                    while(state == CONNECTING_TO_TELEGRAM)
                        {
                            bot->tick();
                            if(!receiveBufferFromMega.isEmpty())
                                bot->sendMessage(receiveBufferFromMega.shift());  // Первый элемент буфера читаем и удаляем
                            sleepTickTime(100); //80
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
                    pinMode(localConf->ESP_LED_PIN, OUTPUT);

                    bot = new FastBot(localConf->BOT_TOKEN);  // Создаем указатель на heap при работе esp
                    bot->setChatID(localConf->ALLOWED_USER);
                    bot->setPeriod(500);

                    bot->attach(std::bind(
                        &App::receiveNewTelegramMessage,
                        this,
                        std::placeholders::_1
                    ));

                    ui.attachBuild(std::bind(
                        &App::buildWebPage,
                        this
                    ));

                    ui.attach(std::bind(
                        &App::action,
                        this
                    ));
                };

            ~App()
                {
                    delete bot;
                };

            void run()
                {   
                    xTaskCreate(readSerialMega, "readSerialMega", 8000, static_cast<void*>(this), 1, &xSerialMegaHandle);
                    xTaskCreate(primaryStateLoop, "primaryStateLoop", 8000, static_cast<void*>(this), 1, &xWifiPingBotHandle);
                    xTaskCreate(secondaryStateLoop, "secondaryStateLoop", 8000, static_cast<void*>(this), 1, &xPingWebHandle);

                };
    };
#endif