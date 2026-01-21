/*
 * WebInterface.h - Web server and REST API interface
 */

#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "P1Parser.h"
#include "P1Modifier.h"
#include "Config.h"

class WebInterface {
public:
  WebInterface(AsyncWebServer* server, P1Parser* parser, P1Modifier* modifier, Config* config);
  
  void begin();
  
private:
  AsyncWebServer* _server;
  P1Parser* _parser;
  P1Modifier* _modifier;
  Config* _config;
  
  // Web page handlers
  void handleRoot(AsyncWebServerRequest* request);
  void handleActualsPage(AsyncWebServerRequest* request);
  void handleSettingsPage(AsyncWebServerRequest* request);
  void handleUpdatePage(AsyncWebServerRequest* request);
  void handleNotFound(AsyncWebServerRequest* request);
  
  // REST API handlers
  void handleGetStatus(AsyncWebServerRequest* request);
  void handleSetMode(AsyncWebServerRequest* request);
  void handleSetPhase(AsyncWebServerRequest* request);
  void handleSetPower(AsyncWebServerRequest* request);
  void handleGetConfig(AsyncWebServerRequest* request);
  void handleGetP1Data(AsyncWebServerRequest* request);
  void handleGetMqttConfig(AsyncWebServerRequest* request);
  void handleSetMqttConfig(AsyncWebServerRequest* request);
  void handleGetAdvancedConfig(AsyncWebServerRequest* request);
  void handleSetAdvancedConfig(AsyncWebServerRequest* request);
  void handleGetEvaConfig(AsyncWebServerRequest* request);
  void handleSetEvaConfig(AsyncWebServerRequest* request);
  
  // Helper functions
  String getActualsPage();
  String getSettingsPage();
  String getUpdatePage();
  String getStatusJSON();
};

#endif // WEBINTERFACE_H
