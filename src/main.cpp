/*
 * ControlZinvoltP1 - ESP32-S3 P1 Port Controller for Zinvolt VT1000
 * 
 * This project controls the P1 dongle to modify smart meter telegrams
 * for sophisticated battery management with the Zinvolt VT1000 system.
 * 
 * Hardware: Universal P1 Port Dongle by Leotro Engineering
 * MCU: ESP32-S3
 * 
 * Features:
 * - Relay/modify smart meter P1 telegrams
 * - TCP P1 port reader
 * - Web interface for monitoring and control
 * - REST API for mode changes and status queries
 * - WiFi configuration portal
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "P1Parser.h"
#include "P1Modifier.h"
#include "WebInterface.h"
#include "Config.h"

// Hardware Serial for P1 Port (adjust pins for ESP32-S3)
#define P1_RX_PIN 44  // GPIO44 (RX)
#define P1_TX_PIN 43  // GPIO43 (TX)
#define P1_SERIAL Serial1

// TCP Server for P1 data streaming
#define P1_TCP_PORT 8888

// Global objects
AsyncWebServer server(80);
AsyncServer tcpServer(P1_TCP_PORT);
WiFiManager wifiManager;
Preferences preferences;

P1Parser p1Parser;
P1Modifier p1Modifier;
WebInterface webInterface(&server, &p1Parser, &p1Modifier);
Config config;

// P1 telegram buffer
String currentP1Telegram = "";
bool telegramComplete = false;

// TCP clients for P1 streaming
std::vector<AsyncClient*> tcpClients;

// Task handles
TaskHandle_t p1ReaderTask;
TaskHandle_t p1RelayTask;

// Function prototypes
void setupWiFi();
void setupWebServer();
void setupTCPServer();
void readP1Task(void* parameter);
void relayP1Task(void* parameter);
void handleNewTCPClient(void* arg, AsyncClient* client);
void broadcastP1Data(const String& telegram);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("ControlZinvoltP1 Starting...");
  Serial.println("=================================");
  
  // Initialize preferences (NVS)
  preferences.begin("czp1", false);
  
  // Load configuration
  config.load(preferences);
  
  // Initialize P1 Serial port
  P1_SERIAL.begin(115200, SERIAL_8N1, P1_RX_PIN, P1_TX_PIN);
  Serial.println("P1 Serial initialized");
  
  // Setup WiFi
  setupWiFi();
  
  // Setup web server
  setupWebServer();
  
  // Setup TCP server for P1 streaming
  setupTCPServer();
  
  // Initialize P1 modifier with config
  p1Modifier.setMode(config.operationMode);
  p1Modifier.setBatteryPhase(config.batteryPhase);
  p1Modifier.setModifyPhase(config.modifyPhase);
  
  // Create tasks for P1 reading and relaying
  xTaskCreatePinnedToCore(
    readP1Task,       // Task function
    "P1Reader",       // Task name
    4096,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    &p1ReaderTask,    // Task handle
    0                 // Core 0
  );
  
  xTaskCreatePinnedToCore(
    relayP1Task,      // Task function
    "P1Relay",        // Task name
    4096,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    &p1RelayTask,     // Task handle
    1                 // Core 1
  );
  
  Serial.println("System ready!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Web Interface: http://");
  Serial.println(WiFi.localIP());
  Serial.print("P1 TCP Port: ");
  Serial.println(P1_TCP_PORT);
}

void loop() {
  // Main loop - most work is done in tasks
  delay(100);
  
  // Periodic status update
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 10000) {
    lastStatusUpdate = millis();
    Serial.printf("Status - Mode: %s, Clients: %d, Uptime: %lu s\n",
                  p1Modifier.getModeString().c_str(),
                  tcpClients.size(),
                  millis() / 1000);
  }
}

void setupWiFi() {
  Serial.println("Setting up WiFi...");
  
  // Set custom AP name
  wifiManager.setConfigPortalTimeout(180); // 3 minutes timeout
  
  // Try to connect to saved WiFi, or start config portal
  if (!wifiManager.autoConnect("ControlZinvoltP1-Setup")) {
    Serial.println("Failed to connect and timeout occurred");
    // Restart and try again
    ESP.restart();
  }
  
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupWebServer() {
  Serial.println("Setting up web server...");
  webInterface.begin();
  server.begin();
  Serial.println("Web server started");
}

void setupTCPServer() {
  Serial.println("Setting up TCP server for P1 streaming...");
  
  tcpServer.onClient([](void* arg, AsyncClient* client) {
    handleNewTCPClient(arg, client);
  }, NULL);
  
  tcpServer.begin();
  Serial.printf("TCP server started on port %d\n", P1_TCP_PORT);
}

void handleNewTCPClient(void* arg, AsyncClient* client) {
  Serial.printf("New TCP client connected: %s\n", client->remoteIP().toString().c_str());
  
  tcpClients.push_back(client);
  
  // Handle client disconnect
  client->onDisconnect([](void* arg, AsyncClient* c) {
    Serial.printf("TCP client disconnected: %s\n", c->remoteIP().toString().c_str());
    
    // Remove from clients list
    tcpClients.erase(
      std::remove(tcpClients.begin(), tcpClients.end(), c),
      tcpClients.end()
    );
  }, NULL);
  
  // Handle client errors
  client->onError([](void* arg, AsyncClient* c, int8_t error) {
    Serial.printf("TCP client error: %s, error: %d\n", 
                  c->remoteIP().toString().c_str(), error);
  }, NULL);
}

void broadcastP1Data(const String& telegram) {
  // Broadcast to all connected TCP clients
  for (auto client : tcpClients) {
    if (client->space() > telegram.length() && client->canSend()) {
      client->add(telegram.c_str(), telegram.length());
      client->send();
    }
  }
}

void readP1Task(void* parameter) {
  String buffer = "";
  
  while (true) {
    // Read incoming P1 data
    while (P1_SERIAL.available()) {
      char c = P1_SERIAL.read();
      buffer += c;
      
      // P1 telegram starts with '/' and ends with '!'
      if (c == '!') {
        // Telegram complete
        if (buffer.startsWith("/")) {
          // Parse the telegram
          p1Parser.parse(buffer);
          
          // Broadcast original data to TCP clients
          broadcastP1Data(buffer);
          
          // Modify telegram based on current mode
          String modifiedTelegram = p1Modifier.modify(buffer, p1Parser);
          
          // Store for relay
          currentP1Telegram = modifiedTelegram;
          telegramComplete = true;
          
          Serial.println("P1 telegram received and processed");
        }
        
        buffer = "";
      }
      
      // Prevent buffer overflow
      if (buffer.length() > 2048) {
        buffer = "";
      }
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void relayP1Task(void* parameter) {
  while (true) {
    // Wait for complete telegram
    if (telegramComplete) {
      // Send modified telegram to P1 output
      P1_SERIAL.print(currentP1Telegram);
      
      telegramComplete = false;
      
      Serial.println("Modified P1 telegram sent");
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
