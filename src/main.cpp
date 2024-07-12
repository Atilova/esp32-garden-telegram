#include <Arduino.h>

#include "config.h"
#include "app/app.h"

App application(conf);

void setup()
    {
        Serial.begin(115200);

        application.setup();
        application.run();
    }

void loop()
    {

    }