/*
 * WebInterface.h - Web server and REST API interface
 */

#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "P1Parser.h"
#include "P1Modifier.h"

class WebInterface {
public:
  WebInterface(AsyncWebServer* server, P1Parser* parser, P1Modifier* modifier);
  
  void begin();
  
private:
  AsyncWebServer* _server;
  P1Parser* _parser;
  P1Modifier* _modifier;
  
  // Web page handlers
  void handleRoot(AsyncWebServerRequest* request);
  void handleNotFound(AsyncWebServerRequest* request);
  
  // REST API handlers
  void handleGetStatus(AsyncWebServerRequest* request);
  void handleSetMode(AsyncWebServerRequest* request);
  void handleSetPhase(AsyncWebServerRequest* request);
  void handleSetPower(AsyncWebServerRequest* request);
  void handleGetConfig(AsyncWebServerRequest* request);
  void handleGetP1Data(AsyncWebServerRequest* request);
  
  // Helper functions
  String getHTMLPage();
  String getStatusJSON();
};

#endif // WEBINTERFACE_H
