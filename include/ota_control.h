#ifndef OTA_CONTROL_H
#define OTA_CONTROL_H

#include <Arduino.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>

extern bool otaUpdateOk;
extern String otaUpdateError;
extern volatile bool otaInProgress;

bool collectRequestBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total, String& body);

void restartAfterOtaTask(void* pvParameters);

#endif
