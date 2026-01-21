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
#include <ArduinoOTA.h>
#include <esp_system.h>

#include "P1Parser.h"
#include "P1Modifier.h"
#include "WebInterface.h"
#include "Config.h"
#include "HomeAssistant.h"
#include "AlphaESSClient.h"
#include "P1Tasks.h"
#include "TCPServer.h"
#include "Version.h"

// Hardware Serial for P1 Port (ESP32-S3)
#define P1_RX_PIN 1   // GPIO1 (Input RX from smart meter)
#define P1_TX_PIN 42  // GPIO42 (Output TX to device)
#define P1_SERIAL Serial1

// Control pins
#define RX_REQ_PIN 2  // GPIO2 (RX request - constant high)
#define TX_REQ_PIN 41 // GPIO41 (TX request - check before sending)
#define LED_PIN 9     // GPIO9 (Status LED)

// TCP Server for P1 data streaming
#define P1_TCP_PORT 9988

// Telnet Server for logging (port 23)
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// TCP Server for P1 streaming (sync WiFiServer to avoid Async init crashes)
WiFiServer tcpServer(P1_TCP_PORT);

// Global objects
AsyncWebServer server(80);
WiFiManager wifiManager;
Preferences preferences;

P1Parser p1Parser;
P1Modifier p1Modifier;
Config config;
WebInterface webInterface(&server, &p1Parser, &p1Modifier, &config);
HomeAssistant homeAssistant(&p1Parser, &p1Modifier, &config);
AlphaESSClient alphaESS(&config);

// Callback for WebInterface to access HomeAssistant
void reconnectMqtt() {
  if (!config.mqttServer.isEmpty()) {
    homeAssistant.setMqttConfig(config.mqttServer.c_str(), config.mqttPort,
                                config.mqttUser.isEmpty() ? NULL : config.mqttUser.c_str(),
                                config.mqttPassword.isEmpty() ? NULL : config.mqttPassword.c_str());
    homeAssistant.reconnectNow();
  }
}

bool getMqttConnected() {
  return homeAssistant.isConnected();
}

// P1 telegram buffer
String currentP1Telegram = "";
bool telegramComplete = false;
bool telegramSent = false;

// TCP clients for P1 streaming
std::vector<WiFiClient> tcpClients;

// Task handles
TaskHandle_t p1ReaderTask;
TaskHandle_t p1RelayTask;

// Function prototypes
void setupWiFi();
void setupOTA();
void setupWebServer();
void setupTelnetServer();
void handleTelnetClient();
void logPrint(const String& msg);
void logPrintln(const String& msg);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.setDebugOutput(true);
  logPrintln("[BOOT] Serial debug enabled (USB)");
  logPrint("[BOOT] Reset reason: ");
  logPrintln(String(static_cast<int>(esp_reset_reason())));
  logPrint("[BOOT] Free heap: ");
  logPrintln(String(ESP.getFreeHeap()));
  
  logPrintln("=================================");
  logPrintln("ControlZinvoltP1 Starting...");
  logPrintln("=================================");
  
  // Initialize preferences (NVS)
  preferences.begin("czp1", false);
  
  // Load configuration
  config.load(preferences);
  
  // Initialize control pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(RX_REQ_PIN, OUTPUT);
  pinMode(TX_REQ_PIN, INPUT);
  
  // Set RX request constant high (required by smart meter)
  digitalWrite(RX_REQ_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  
  logPrintln("Control pins initialized");
  
  // Initialize P1 Serial port with large RX buffer to prevent data loss
  // P1 telegrams can be 1KB+, so we need a buffer that can hold multiple telegrams
  // NOTE: setRxBufferSize must be called BEFORE begin()
  P1_SERIAL.setRxBufferSize(4096);  // 4KB RX buffer
  P1_SERIAL.begin(115200, SERIAL_8N1, P1_RX_PIN, P1_TX_PIN);
  logPrintln("P1 Serial initialized with 4KB RX buffer");
  
  // Setup WiFi
  setupWiFi();
  
  // Setup OTA updates
  setupOTA();
  
  // Setup Home Assistant MQTT (configure via web interface or here)
  if (!config.mqttServer.isEmpty()) {
    const char* user = config.mqttUser.isEmpty() ? NULL : config.mqttUser.c_str();
    const char* pass = config.mqttPassword.isEmpty() ? NULL : config.mqttPassword.c_str();
    
    logPrintln("Initializing MQTT with saved settings:");
    logPrint("  Server: ");
    logPrint(config.mqttServer);
    logPrint(":");
    logPrintln(String(config.mqttPort));
    logPrint("  User: ");
    logPrintln(user ? user : "(none)");
    
    homeAssistant.begin(config.mqttServer.c_str(), config.mqttPort, user, pass);
    logPrintln("MQTT configured from settings");
  } else {
    logPrintln("MQTT not configured - use web interface to set up");
  }
  
  // Initialize AlphaESS client for battery data
  if (config.evaEnabled && !config.evaSerialNumber.isEmpty()) {
    alphaESS.begin();
    logPrintln("AlphaESS client initialized:");
    logPrint("  Serial: ");
    logPrintln(config.evaSerialNumber);
  } else {
    logPrintln("AlphaESS integration disabled - enable in settings");
  }
  
  // Setup web server
  setupWebServer();
  
  // Setup TCP server for P1 streaming
  setupTCPServer();
  
  // Setup telnet server for logging
  setupTelnetServer();
  
  // Initialize P1 modifier with config
  p1Modifier.setMode(config.operationMode);
  p1Modifier.setBatteryPhase(config.batteryPhase);
  p1Modifier.setModifyPhase(config.modifyPhase);
  
  // Create tasks for P1 reading and relaying
  xTaskCreatePinnedToCore(
    readP1Task,       // Task function
    "P1Reader",       // Task name
    8192,             // Stack size (8KB for larger buffers)
    NULL,             // Parameters
    2,                // Priority (higher = more important)
    &p1ReaderTask,    // Task handle
    0                 // Core 0
  );
  
  xTaskCreatePinnedToCore(
    relayP1Task,      // Task function
    "P1Relay",        // Task name
    8192,             // Stack size (8KB)
    NULL,             // Parameters
    2,                // Priority
    &p1RelayTask,     // Task handle
    1                 // Core 1
  );
  
  logPrintln("System ready!");
  logPrint("IP Address: ");
  logPrintln(WiFi.localIP().toString());
  logPrint("Web Interface: http://");
  logPrintln(WiFi.localIP().toString());
  logPrint("P1 TCP Port: ");
  logPrintln(String(P1_TCP_PORT));
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // DEBUG: Check if homeAssistant.loop() is callable
  static unsigned long lastMqttCheck = 0;
  if (millis() - lastMqttCheck > 10000) {
    Serial.print("[MAIN] Checking MQTT - Connected: ");
    Serial.print(homeAssistant.isConnected());
    Serial.print(", Config server: ");
    Serial.println(config.mqttServer);
    lastMqttCheck = millis();
  }
  
  // Handle Home Assistant MQTT
  homeAssistant.loop();
  
  // Handle telnet logging client
  handleTelnetClient();
  
  // Main loop - most work is done in tasks
  delay(100);
  
  // Periodic status update
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 10000) {
    lastStatusUpdate = millis();
    logPrint("Status - Mode: ");
    logPrint(p1Modifier.getModeString());
    logPrint(", TCP: ");
    logPrint(String(getTCPClientCount()));
    logPrint(", MQTT: ");
    logPrint(homeAssistant.isConnected() ? "OK" : "X");
    logPrint(" (server: ");
    logPrint(config.mqttServer.isEmpty() ? "not set" : config.mqttServer);
    logPrint("), Uptime: ");
    logPrint(String(millis() / 1000));
    logPrintln(" s");
  }
}

void setupWiFi() {
  logPrintln("Setting up WiFi...");
  
  // Set custom AP name
  wifiManager.setConfigPortalTimeout(180); // 3 minutes timeout
  
  // Try to connect to saved WiFi, or start config portal
  if (!wifiManager.autoConnect("ControlZinvoltP1-Setup")) {
    logPrintln("Failed to connect and timeout occurred");
    // Restart and try again
    ESP.restart();
  }
  
  logPrintln("WiFi connected!");
  logPrint("IP address: ");
  logPrintln(WiFi.localIP().toString());
  Serial.println("[WIFI] Connected");
  Serial.print("[WIFI] SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WIFI] RSSI: ");
  Serial.println(WiFi.RSSI());
}

void setupOTA() {
  logPrintln("Setting up OTA updates...");
  
  // Set OTA hostname
  ArduinoOTA.setHostname("ControlZinvoltP1");
  
  // Optional: Set OTA password for security
  // ArduinoOTA.setPassword("your-password-here");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_SPIFFS
      type = "filesystem";
    }
    logPrintln("Start updating " + type);
    
    // Stop P1 tasks during update to prevent interference
    if (p1ReaderTask != NULL) {
      vTaskSuspend(p1ReaderTask);
    }
    if (p1RelayTask != NULL) {
      vTaskSuspend(p1RelayTask);
    }
  });
  
  ArduinoOTA.onEnd([]() {
    logPrintln("\nOTA Update Complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    // Print progress every 500ms
    if (now - lastPrint > 500) {
      logPrint("Progress: ");
      logPrint(String(progress / (total / 100)));
      logPrintln("%");
      lastPrint = now;
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    logPrint("Error[");
    logPrint(String(error));
    logPrint("]: ");
    if (error == OTA_AUTH_ERROR) {
      logPrintln("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      logPrintln("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      logPrintln("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      logPrintln("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      logPrintln("End Failed");
    }
    
    // Resume P1 tasks if OTA failed
    if (p1ReaderTask != NULL) {
      vTaskResume(p1ReaderTask);
    }
    if (p1RelayTask != NULL) {
      vTaskResume(p1RelayTask);
    }
  });
  
  ArduinoOTA.begin();
  logPrintln("OTA updates enabled");
  logPrint("OTA Hostname: ");
  logPrintln(WiFi.localIP().toString());
}

void setupWebServer() {
  logPrintln("Setting up web server...");
  webInterface.begin();
  server.begin();
  logPrintln("Web server started");
}

void setupTelnetServer() {
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  logPrintln("=================================");
  logPrintln("Telnet logging server started on port 23");
  logPrintln("Connect with: telnet " + WiFi.localIP().toString());
  logPrintln("=================================");
}

void handleTelnetClient() {
  // Check for new telnet connection
  if (telnetServer.hasClient()) {
    // Disconnect old client if present
    if (telnetClient && telnetClient.connected()) {
      telnetClient.stop();
    }
    telnetClient = telnetServer.available();
    logPrintln("\n*** Telnet client connected: " + telnetClient.remoteIP().toString() + " ***");
  }
  
  // Clean up disconnected client
  if (telnetClient && !telnetClient.connected()) {
    telnetClient.stop();
  }
}

// Helper function to print to both Serial and Telnet
void logPrint(const String& msg) {
  Serial.print(msg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.print(msg);
  }
}

void logPrintln(const String& msg) {
  Serial.println(msg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(msg);
  }
}


