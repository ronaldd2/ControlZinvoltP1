/*
 * HomeAssistant.h - MQTT Home Assistant Integration
 */

#ifndef HOMEASSISTANT_H
#define HOMEASSISTANT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "P1Parser.h"
#include "P1Modifier.h"
#include "Config.h"

// Forward declarations for logging functions
void logPrint(const String& msg);
void logPrintln(const String& msg);

class HomeAssistant {
public:
  HomeAssistant(P1Parser* parser, P1Modifier* modifier, Config* config);
  
  // Initialize MQTT connection
  void begin(const char* mqttServer, int mqttPort = 1883, 
             const char* mqttUser = NULL, const char* mqttPassword = NULL);
  
  // Main loop - handle MQTT
  void loop();
  
  // Check if connected
  bool isConnected();
  
  // Publish all sensor data
  void publishSensors();
  
  // Set MQTT server configuration
  void setMqttConfig(const char* server, int port, const char* user, const char* pass);
  
  // Reconnect with new settings
  void reconnectNow();
  
private:
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;
  P1Parser* _parser;
  P1Modifier* _modifier;
  Config* _config;
  
  // MQTT settings
  String _mqttServer;
  int _mqttPort;
  String _mqttUser;
  String _mqttPassword;
  String _deviceId;
  
  // Timing
  unsigned long _lastReconnect;
  unsigned long _lastPublish;
  unsigned long _lastDiscovery;
  
  // Connection management
  void reconnect();
  void publishDiscovery();
  void handleCommand(char* topic, byte* payload, unsigned int length);
  
  // Helper functions
  String getBaseTopic();
  String getDiscoveryTopic(const char* component, const char* objectId);
  void publishSensor(const char* name, const char* objectId, const char* deviceClass, 
                     const char* unit, const char* icon);
  void publishBinarySensor(const char* name, const char* objectId, const char* deviceClass);
  void publishSwitch(const char* name, const char* objectId, const char* icon);
  void publishSelect(const char* name, const char* objectId, const char* icon);
  void publishNumber(const char* name, const char* objectId, const char* icon,
                     float min, float max, float step, const char* unit);
};

#endif // HOMEASSISTANT_H
