#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>
#include <iostream>
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPIFFS.h>
#include <IPAddress.h>

#include <ESP32Ping.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <CircularBuffer.h>
#include <AsyncEventSource.h>
#include <ESPAsyncWebServer.h>

#include "docs.h"
#include "utils.h"
#include "config.h"
#include "constants.h"

using namespace AppConstants;


class App
    {
        private:
            AppConfig* localConf;
            State state = State::WAKE_WIFI;
            AuxilaryValues values;

            TaskHandle_t xMqttHandle = NULL;
            TaskHandle_t xWebServerHandle = NULL;
            TaskHandle_t xInternetHandle = NULL;
            TaskHandle_t xSerialMegaHandle = NULL;

            WiFiClient wifiClient;
            PubSubClient mqttClient;

            AsyncWebServer* webServer;
            AsyncEventSource *webSourceEvents;

            StaticJsonDocument<500> jsonBuffer;
            CircularBuffer<const char*, 15> exchangeBuffer; // Stores char[] pointers on heap.

            /**
             * State and task handlers.
             */

            bool isState(State expected)
                {
                   return state == expected;
                }

            static void primaryStateLoop(void* parameter)
				{
					App* _app = static_cast<App*>(parameter);
					for(;;)
						{
							switch(_app->state)
								{
									case State::WAKE_WIFI:
										{
											_app->wakeWifi();
											_app->state = State::INITIATE_MQTT_CONNECTION;
											break;
										}
									case State::INITIATE_MQTT_CONNECTION:
										{
											_app->state = _app->initiateMqttConnection()
												? State::CONSUME_MQTT
												: WiFi.isConnected()
													? State::CONSUME_WEB_SERVER
													: State::WAKE_WIFI;

											break;
										}
									case State::CONSUME_MQTT:
										{
											_app->consumeMqtt();
											_app->state = State::INITIATE_MQTT_CONNECTION;

											break;
										}
									case State::CONSUME_WEB_SERVER:
										{
											while (WiFi.isConnected() && !_app->initiateMqttConnection()) {
												wait(4000);
											}

											_app->state = WiFi.isConnected()
												? State::CONSUME_MQTT
												: State::WAKE_WIFI;

											break;
										}
									default:
										{
											wait(500); // Prevents kernel prohibition.
											break;
										}

								}
						}
				}

			static void secondaryStateLoop(void* parameter)
                {
                    App* _app = static_cast<App*>(parameter);
                    for(;;)
                        {
                            switch(_app->state)
                                {
                                    // Не получается подключиться к Mqtt серверу, переходим на локальный WebServer.
                                    case State::CONSUME_WEB_SERVER:
                                        {
                                            _app->startWebServer();
                                            break;
                                        }
                                    default:
                                        {
                                            wait(500); // Prevents kernel prohibition.
                                            break;
                                        }
                                }
                        }
                }

            static void readSerialMEGA(void* parameter)
                {
                    constexpr static const char RESPONSE_END = '%',
                                                SKIP_LINE = '\r';
                    String ttyData;
                    App* _app = static_cast<App*>(parameter);

                    for(;;)
                        {
                            while(_app->localConf->MEGA_IO.available())
                                {
                                    char nextCharacter = _app->localConf->MEGA_IO.read();
                                    if(nextCharacter == SKIP_LINE) {
                                        continue;
                                    }

                                    if(nextCharacter == RESPONSE_END)
                                        {
                                            _app->addBufferMessage(ttyData);
                                            ttyData.clear();
                                            continue;
                                        }
                                    ttyData += nextCharacter;
                                }
                        }
                }

            static void updateInternetStatus(void* parameter)
                {

                }

            /**
             * Buffer interaction methods.
             */

            void addBufferMessage(const String& buffer)
                {
                    if(!exchangeBuffer.available()) {
                        return;
                    }

                    char *heapBuffer = new char[buffer.length() + 1]; // Динамическое выделение памяти на heap.
                    strcpy(heapBuffer, buffer.c_str()); // Записываем данные в буфер.
                    exchangeBuffer.push(heapBuffer); // Добавляем указатель в буфер на созданный массив.
                };

            void processBufferMessage()
                {
                    if(exchangeBuffer.isEmpty()) {
                        return;
                    }

                    const char* message = exchangeBuffer.shift();
                    if(!strcmp(message, MegaCodes::INET_REQUEST))
                        {
                            syncMEGAInternetAccess(); // Синхронизация состояния интернета для меги.
                        }
                    else if(!strcmp(message, MegaCodes::NTP_REQUEST))
                        {
                            syncMEGADatetime(); // Синхронизация времени и дня недели для меги.
                        }
                    else
                        {
                            deliverUser(message); // Все остальное выводим в телеграм или в web.
                        }

                    delete [] message; // Cleanup heap, освобождаем память.
                };

            /**
             * WiFi and internet interaction methods.
             */

            void shutdownWifi()
                {
                    WiFi.disconnect(); // Обрываем WIFI соединения.
                    WiFi.softAPdisconnect(); // Отключаем точку доступа, т.е режим роутера.
                    WiFi.mode(WIFI_OFF); // Отключаем WIFI.
                    wait(500);
                };

            void wakeWifi()
                {
                    digitalWrite(localConf->ESP_LED_PIN, LOW);
                    while(!WiFi.isConnected())
                        {
                            shutdownWifi();
                            repeat(3)
                                {
                                    if(WiFi.isConnected()) {
                                        break; // Если подключились выходим.
                                    }

                                    WiFi.mode(WIFI_STA); // Режим клиента.
                                    WiFi.config(
                                        localConf->ESP_IP,
                                        localConf->WIFI_GATEWAY,
                                        localConf->WIFI_SUBNET_MASK,
                                        localConf->WIFI_PRIMARY_DNS
                                    );
                                    WiFi.begin(localConf->WIFI_SSID, localConf->WIFI_PASSWORD);
                                    wait(5000);
                                }
                        }
                    digitalWrite(localConf->ESP_LED_PIN, HIGH);
                    Serial.println("WiFi connected.");
                }

            bool hasInternetAccess()
                {
                    for(uint8_t index=0; index < localConf->PING_HOSTS_LENGTH; index++)
                        {
                            IPAddress host = localConf->PING_HOSTS[index];
                            if(Ping.ping(host, 2)) {
                                return true;
                            }
                        }
                    return false;
                }

            /**
             * MEGA interaction methods.
             */

            void deliverMEGA(const char* payload)
                {
                    localConf->MEGA_IO.println(payload);
                }

            void syncMEGADatetime()
                {
                    syncESPTimezone();

                    tm datetime;
                    if(!getLocalTime(&datetime))
                        return addBufferMessage(UserMessages::NTP_SYNC_FAIL);

                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "day=%d", datetime.tm_wday);
                    deliverMEGA(buffer);

                    snprintf(buffer, sizeof(buffer), "time=%02d:%02d:%02d", datetime.tm_hour, datetime.tm_min, datetime.tm_sec);
                    deliverMEGA(buffer);
                }

            void syncMEGAInternetAccess()
                {
                    deliverMEGA(values.hasInternetAccess
                        ? MegaCodes::INET_OK
                        : MegaCodes::INET_NO
                    );
                }

            /**
             * Timezone specific code.
             */

            void setupTimezone()
                {
                    unsetenv("TZ");
                    setenv("TZ", localConf->ESP_TIMEZONE, 1);
                    tzset();
                    values.tzEnvPointer = getenv("TZ"); // Instead of using setenv (memory leak), use this pointer and strcpy.
                }

            void syncESPTimezone()
                {
                    if(!values.hasInternetAccess) {
                        return;
                    }

                    configTime(0, 0, "pool.ntp.org"); // Синхронизация времени с ntp сервером.
                    strcpy(values.tzEnvPointer, localConf->ESP_TIMEZONE);
                    tzset();
                }

            /**
             * Mqtt consumer specific code.
             */

            void onMqttInboxMessage(char* topic, byte* payload, uint8_t length)
                {
					std::cout << "Inbox: " << topic << std::endl;
                }

            void setupMqttClient()
                {
                    mqttClient.setClient(wifiClient);
                    mqttClient.setServer(localConf->MQTT_HOST, localConf->MQTT_PORT);
                    mqttClient.setKeepAlive(localConf->MQTT_KEEP_ALIVE);
                    mqttClient.setSocketTimeout(localConf->MQTT_TIMEOUT);
                    mqttClient.setCallback(std::bind(
                        &App::onMqttInboxMessage,
                        this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3
                    ));
                }

            bool initiateMqttConnection()
                {
					bool pined = Ping.ping(localConf->MQTT_HOST, 3);

                    bool connectivity = pined && mqttClient.connect(
						localConf->MQTT_CLIENT_ID,
						localConf->MQTT_USERNAME,
						localConf->MQTT_PASSWORD
					);

                    std::cout << "TRYING - Ping " << pined << " Connection: " << connectivity << " | " << std::endl;
                    return pined && connectivity;
                }

			void consumeMqtt()
				{
					std::cout << "Mqtt connected." << std::endl;
					exchangeBuffer.clear();
					mqttClient.subscribe(localConf->MQTT_RECEIVE_TOPIC);

					while(isState(State::CONSUME_MQTT) && mqttClient.loop()) {
						processBufferMessage();
						wait(100);
					}
				}

            /**
             * WebServer specific code.
             */

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
                };

            void webServerHandleBodyRequest(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
                {
                    if(request->url() != "/send" || request->method() != HTTP_POST) {
                        return request->send(400);
                    }

                    DeserializationError error = deserializeJson(jsonBuffer, (const char*)data);
                    if(error) {
                        return request->send(400);
                    }

                    char buffer[500];
                    serializeJson(jsonBuffer, buffer); // strcpy(buffer, (const char*)data);
                    String message = jsonBuffer["message"].as<String>();

                    webSourceEvents->send(buffer, WebServerEvents::USER_MESSAGE, millis());
                    handleUserMessage(message);
                    return request->send(200);
                };

            void startWebServer()
                {
                    webServer->begin();
                    Serial.println("WebServer started.");

                    uint32_t lastPingTime = millis() + localConf->WEB_SERVER_EVENT_SOURCE_PING_INTERVAL;
                    while(isState(State::CONSUME_WEB_SERVER))
                        {
                            if(lastPingTime < millis())
                                {
                                    webSourceEvents->send("", WebServerEvents::PING_MESSAGE, millis());
                                    lastPingTime = millis() + localConf->WEB_SERVER_EVENT_SOURCE_PING_INTERVAL;
                                }

                            processBufferMessage();
                            wait(100);
                        }

                    webSourceEvents->close();
                    webServer->end();
                    wait(5000); // Do not remove, wait for graceful server end, before tcpCleanup().
                    tcpCleanup();
                }

            /**
             * Shared consumer methods and handlers.
             */

            void deliverUser(const char* message)
                {
                    if(isState(State::CONSUME_WEB_SERVER)) {
                        std::cout << "SEND WEB" << std::endl;
                        return webSourceEvents->send(message, WebServerEvents::MEGA_MESSAGE, millis());
                    }

                    mqttClient.publish(localConf->MQTT_PUBLISH_TOPIC, message);
                }

            void sendHelpPages(uint8_t startIndex, uint8_t endIndex)
                {
                    for(uint8_t index = startIndex; index <= endIndex; index++) {
                        addBufferMessage(helpCommand[index]);
                    }
                }

            void sendESPFreeHeap()
                {
                    char buffer[32];
                    sprintf(buffer, "ESP free heap = %d", ESP.getFreeHeap());
                    addBufferMessage(buffer);
                }

            void handleUserMessage(String& message)
                {
                    if(message == UserCommands::HELP) {
                        return sendHelpPages(0, 3);
                    }

                    if(message == UserCommands::HELP_POWER) {
                        return sendHelpPages(4, 5);
                    }

                    if(message == UserCommands::MEMORY) {
                        return sendESPFreeHeap();
                    }

                    if(message == UserCommands::REBOOT)
                        {
                            deliverUser(UserMessages::ESP_REBOOT);
                            wait(1000);
                            ESP.restart();
                        };

                    deliverMEGA(message.c_str());
                };


        public:
            /**
             * Public interface.
             */

            App(AppConfig& data)
                {
                    localConf = &data;

                    setupTimezone();
                    setupMqttClient();
                    setupWebServer();
                }

			~App()
                {
                    delete webServer;
                    delete webSourceEvents;

					while(!exchangeBuffer.isEmpty())
						{
							const char* unhandled = exchangeBuffer.pop();
							delete [] unhandled;
						}
                }

			void setup()
				{
					SPIFFS.begin();
					localConf->MEGA_IO.begin(115200); // Это Serial2.begin(115200).
					pinMode(localConf->ESP_LED_PIN, OUTPUT);
				}

            void run()
                {
                    std::cout << "TEST: " << localConf->MQTT_HOST << " | " << localConf->MQTT_RECEIVE_TOPIC << std::endl;

                    xTaskCreate(primaryStateLoop, "primaryStateLoop", 8000, static_cast<void*>(this), 1, &xMqttHandle);
                    xTaskCreate(secondaryStateLoop, "secondaryStateLoop", 16000, static_cast<void*>(this), 1, &xWebServerHandle);
                    // xTaskCreate(updateInternetStatus, "updateInternetStatus", 8000, static_cast<void*>(this), 1, &xInternetHandle);
                    xTaskCreate(readSerialMEGA, "readSerialMEGA", 8000, static_cast<void*>(this), 1, &xSerialMegaHandle);
                }
    };

#endif