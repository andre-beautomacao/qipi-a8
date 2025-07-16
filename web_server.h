#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

void serverRoutes();

#endif // WEB_SERVER_H
