/*
 * HomeAssistant.cpp - Implementation of MQTT Home Assistant Integration
 */

#include "HomeAssistant.h"

HomeAssistant::HomeAssistant(P1Parser* parser, P1Modifier* modifier, Config* config) {
  _parser = parser;
  _modifier = modifier;
  _config = config;
  _mqttClient.setClient(_wifiClient);
  _lastReconnect = 0;
  _lastPublish = 0;
  _lastDiscovery = 0;
  _mqttPort = 1883;
  _publishRequested = false;
  
  // Generate unique device ID from MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  _deviceId = "controlzinvoltp1_" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
}

void HomeAssistant::begin(const char* mqttServer, int mqttPort, 
                           const char* mqttUser, const char* mqttPassword) {
  setMqttConfig(mqttServer, mqttPort, mqttUser, mqttPassword);
  
  _mqttClient.setServer(mqttServer, mqttPort);
  _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->handleCommand(topic, payload, length);
  });
  
  Serial.println("Home Assistant MQTT initialized");
  Serial.printf("  Server: %s:%d\n", mqttServer, mqttPort);
  Serial.printf("  Device ID: %s\n", _deviceId.c_str());
}

void HomeAssistant::setMqttConfig(const char* server, int port, const char* user, const char* pass) {
  _mqttServer = server ? server : "";
  _mqttPort = port;
  _mqttUser = user ? user : "";
  _mqttPassword = pass ? pass : "";
}

void HomeAssistant::reconnectNow() {
  if (_mqttClient.connected()) {
    _mqttClient.disconnect();
  }
  _lastReconnect = 0; // Force immediate reconnect
}

void HomeAssistant::loop() {
  static unsigned long lastConnCheck = 0;
  unsigned long now = millis();
  
  // Check connection status more frequently
  if (now - lastConnCheck > 1000) {
    lastConnCheck = now;
    // Force PubSubClient to check socket status
    if (_mqttClient.connected()) {
      _mqttClient.loop();  // This will detect broken connections
    }
  }
  
  if (!_mqttClient.connected()) {
    if (now - _lastReconnect > 5000) {
      _lastReconnect = now;
      logPrintln("[MQTT] Reconnecting...");
      reconnect();
    }
  } else {
    // Publish only when a new P1 telegram was processed
    if (_publishRequested && (now - _lastPublish > 200)) { // throttle to avoid bursts
      _publishRequested = false;
      _lastPublish = now;
      publishSensors();
    }
    
    // Republish discovery every 5 minutes
    if (now - _lastDiscovery > 300000) {
      _lastDiscovery = now;
      publishDiscovery();
    }
  }
}

void HomeAssistant::requestPublish() {
  _publishRequested = true;
}

bool HomeAssistant::isConnected() {
  return _mqttClient.connected();
}

void HomeAssistant::reconnect() {
  if (_mqttServer.isEmpty()) {
    Serial.println("[MQTT] Server not configured!");
    return;
  }
  
  Serial.print("[MQTT] Connecting to ");
  Serial.print(_mqttServer);
  Serial.print(":");
  Serial.println(_mqttPort);
  
  String clientId = _deviceId + "_" + String(random(0xffff), HEX);
  
  bool connected;
  if (_mqttUser.isEmpty()) {
    connected = _mqttClient.connect(clientId.c_str());
  } else {
    connected = _mqttClient.connect(clientId.c_str(), _mqttUser.c_str(), _mqttPassword.c_str());
  }
  
  if (connected) {
    Serial.println("[MQTT] Connected!");
    
    // Publish availability (online)
    String statusTopic = getBaseTopic() + "/status";
    _mqttClient.publish(statusTopic.c_str(), "online", true);
    Serial.println("[MQTT] Published availability: online");
    
    String cmdTopic = getBaseTopic() + "/cmd/#";
    _mqttClient.subscribe(cmdTopic.c_str());
    publishDiscovery();
    publishSensors();
  } else {
    Serial.print("[MQTT] Failed, rc=");
    Serial.println(_mqttClient.state());
  }
}

String HomeAssistant::getBaseTopic() {
  return "controlzinvoltp1/" + _deviceId;
}

String HomeAssistant::getDiscoveryTopic(const char* component, const char* objectId) {
  return "homeassistant/" + String(component) + "/" + _deviceId + "/" + String(objectId) + "/config";
}

void HomeAssistant::publishDiscovery() {
  Serial.println("Publishing Home Assistant discovery messages...");
  
  // Device information (common for all entities)
  JsonDocument deviceDoc;
  JsonArray identifiers = deviceDoc["identifiers"].to<JsonArray>();
  identifiers.add(_deviceId);
  deviceDoc["name"] = "ControlZinvolt P1";
  deviceDoc["model"] = "ESP32-S3 P1 Controller";
  deviceDoc["manufacturer"] = "Leotro Engineering";
  deviceDoc["sw_version"] = "1.0.0";
  
  String deviceJson;
  serializeJson(deviceDoc, deviceJson);
  
  // === Power Sensors ===
  publishSensor("Total Power", "total_power", "power", "W", "mdi:flash");
  publishSensor("L1 Power", "l1_power", "power", "W", "mdi:flash");
  publishSensor("L2 Power", "l2_power", "power", "W", "mdi:flash");
  publishSensor("L3 Power", "l3_power", "power", "W", "mdi:flash");
  
  // === Current Sensors ===
  publishSensor("L1 Current", "l1_current", "current", "A", "mdi:current-ac");
  publishSensor("L2 Current", "l2_current", "current", "A", "mdi:current-ac");
  publishSensor("L3 Current", "l3_current", "current", "A", "mdi:current-ac");
  
  // === Voltage Sensors ===
  publishSensor("L1 Voltage", "l1_voltage", "voltage", "V", "mdi:sine-wave");
  publishSensor("L2 Voltage", "l2_voltage", "voltage", "V", "mdi:sine-wave");
  publishSensor("L3 Voltage", "l3_voltage", "voltage", "V", "mdi:sine-wave");
  
  // === Energy Sensors ===
  publishSensor("Energy Import", "energy_import", "energy", "kWh", "mdi:transmission-tower-import");
  publishSensor("Energy Export", "energy_export", "energy", "kWh", "mdi:transmission-tower-export");
  
  // === System Sensors ===
  publishSensor("WiFi Signal", "wifi_rssi", "signal_strength", "dBm", "mdi:wifi");
  publishSensor("Uptime", "uptime", NULL, "s", "mdi:timer-outline");
  publishSensor("Free Heap", "free_heap", NULL, "bytes", "mdi:memory");
  
  // === Battery Numbers (controllable from HA) ===
  publishNumber("Battery SOC", "battery_soc", "mdi:battery", 0, 100, 0.1, "%");
  publishNumber("Battery Power", "battery_power", "mdi:battery-charging", -20000, 20000, 1, "W");
  publishNumber("Battery Capacity", "battery_capacity", "mdi:battery-high", 0, 100, 0.1, "kWh");
  publishNumber("Battery Production", "battery_production", "mdi:solar-power", 0, 50000, 1, "W");
  publishNumber("Battery Consumption", "battery_consumption", "mdi:transmission-tower", 0, 50000, 1, "W");
  
  // === Operation Mode Select ===
  publishSelect("Operation Mode", "operation_mode", "mdi:cog");
  
  // === Phase Selectors ===
  publishSelect("Battery Phase", "battery_phase", "mdi:electric-switch");
  publishSelect("Modify Phase", "modify_phase", "mdi:electric-switch");
  
  // === Force Power Number ===
  publishNumber("Force Power", "force_power", "mdi:flash-auto", 0, 20000, 100, "W");
  
  // === Status Binary Sensor ===
  publishBinarySensor("P1 Data Valid", "p1_valid", "connectivity");

  // === Daily Energy Sensors ===
  publishSensor("Daily Energy Import", "daily_energy_import", "energy", "kWh", "mdi:counter");
  publishSensor("Daily Energy Export", "daily_energy_export", "energy", "kWh", "mdi:counter");
  
  Serial.println("Home Assistant discovery published");
}

void HomeAssistant::publishSensor(const char* name, const char* objectId, 
                                   const char* deviceClass, const char* unit, const char* icon) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = _deviceId + "_" + String(objectId);
  doc["state_topic"] = getBaseTopic() + "/state";
  doc["value_template"] = "{{ value_json." + String(objectId) + " }}";
  
  if (deviceClass) doc["device_class"] = deviceClass;
  if (unit) doc["unit_of_measurement"] = unit;
  if (icon) doc["icon"] = icon;
  
  doc["availability_topic"] = getBaseTopic() + "/status";
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  
  // Add device info
  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(_deviceId);
  device["name"] = "ControlZinvolt P1";
  device["model"] = "ESP32-S3 P1 Controller";
  device["manufacturer"] = "AI";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("sensor", objectId);
  _mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishBinarySensor(const char* name, const char* objectId, const char* deviceClass) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = _deviceId + "_" + String(objectId);
  doc["state_topic"] = getBaseTopic() + "/state";
  doc["value_template"] = "{{ value_json." + String(objectId) + " }}";
  doc["payload_on"] = "true";
  doc["payload_off"] = "false";
  
  if (deviceClass) doc["device_class"] = deviceClass;
  
  doc["availability_topic"] = getBaseTopic() + "/status";
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  
  // Add device info
  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(_deviceId);
  device["name"] = "ControlZinvolt P1";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("binary_sensor", objectId);
  _mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishSelect(const char* name, const char* objectId, const char* icon) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = _deviceId + "_" + String(objectId);
  doc["state_topic"] = getBaseTopic() + "/state";
  doc["command_topic"] = getBaseTopic() + "/cmd/" + String(objectId);
  
  if (strcmp(objectId, "operation_mode") == 0) {
    doc["value_template"] = "{{ value_json.operation_mode }}";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("Unmodified Forward");
    options.add("Off");
    options.add("Force Charge");
    options.add("Force Discharge");
    options.add("Charge Only");
    options.add("Discharge Only");
  } else if (strcmp(objectId, "battery_phase") == 0) {
    doc["value_template"] = "{{ value_json.battery_phase }}";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("L1");
    options.add("L2");
    options.add("L3");
  } else if (strcmp(objectId, "modify_phase") == 0) {
    doc["value_template"] = "{{ value_json.modify_phase }}";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("L1");
    options.add("L2");
    options.add("L3");
  }
  
  if (icon) doc["icon"] = icon;
  
  doc["availability_topic"] = getBaseTopic() + "/status";
  
  // Add device info
  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(_deviceId);
  device["name"] = "ControlZinvolt P1";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("select", objectId);
  _mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishNumber(const char* name, const char* objectId, const char* icon,
                                   float min, float max, float step, const char* unit) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = _deviceId + "_" + String(objectId);
  doc["state_topic"] = getBaseTopic() + "/state";
  doc["command_topic"] = getBaseTopic() + "/cmd/" + String(objectId);
  doc["value_template"] = "{{ value_json." + String(objectId) + " }}";
  doc["min"] = min;
  doc["max"] = max;
  doc["step"] = step;
  
  if (unit) doc["unit_of_measurement"] = unit;
  if (icon) doc["icon"] = icon;
  
  doc["availability_topic"] = getBaseTopic() + "/status";
  
  // Add device info
  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(_deviceId);
  device["name"] = "ControlZinvolt P1";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("number", objectId);
  _mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishSensors() {
  if (!_mqttClient.connected()) {
    logPrintln("[MQTT] publishSensors: not connected!");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    logPrintln("[MQTT] publishSensors: WiFi not connected!");
    return;
  }
  
  logPrintln("[MQTT] Publishing sensor data...");
  
  JsonDocument doc;
  
  // Cache values to ensure consistent sign handling per publish cycle
  float l1Power = _parser->getActivePowerL1();
  float l2Power = _parser->getActivePowerL2();
  float l3Power = _parser->getActivePowerL3();
  float totalPower = l1Power + l2Power + l3Power;
  float l1Current = _parser->getCurrentL1();
  float l2Current = _parser->getCurrentL2();
  float l3Current = _parser->getCurrentL3();
  float v1 = _parser->getVoltageL1();
  float v2 = _parser->getVoltageL2();
  float v3 = _parser->getVoltageL3();
  
  // P1 Power data
  doc["total_power"] = round(totalPower * 1000);
  doc["l1_power"] = round(l1Power * 1000);
  doc["l2_power"] = round(l2Power * 1000);
  doc["l3_power"] = round(l3Power * 1000);
  
  // Current data
  doc["l1_current"] = (l1Power >= 0 ? l1Current : -l1Current);
  doc["l2_current"] = (l2Power >= 0 ? l2Current : -l2Current);
  doc["l3_current"] = (l3Power >= 0 ? l3Current : -l3Current);
  
  // Voltage data
  doc["l1_voltage"] = v1;
  doc["l2_voltage"] = v2;
  doc["l3_voltage"] = v3;
  
  // Energy data
  doc["energy_import"] = _parser->getTotalEnergyImport();
  doc["energy_export"] = _parser->getTotalEnergyExport();

  // Daily energy (relative to start of day baselines)
  float dailyImport = 0.0f;
  float dailyExport = 0.0f;
  if (_config->dayStartEnergyImport > 0.0f) {
    dailyImport = max(0.0f, _parser->getTotalEnergyImport() - _config->dayStartEnergyImport);
  }
  if (_config->dayStartEnergyExport > 0.0f) {
    dailyExport = max(0.0f, _parser->getTotalEnergyExport() - _config->dayStartEnergyExport);
  }
  doc["daily_energy_import"] = dailyImport;
  doc["daily_energy_export"] = dailyExport;
  
  // System data
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  
  // P1 valid status (send as string to match binary_sensor payload_on/payload_off)
  doc["p1_valid"] = _parser->isValid() ? "true" : "false";
  
  // Battery data (from config)
  doc["battery_soc"] = _config->batterySOC;
  doc["battery_power"] = _config->batteryPower;
  doc["battery_capacity"] = _config->batteryCapacity;
  doc["battery_production"] = _config->batteryProduction;
  doc["battery_consumption"] = _config->batteryConsumption;
  
  // Operation mode
  String modeStr = _modifier->getModeString();
  doc["operation_mode"] = modeStr;
  
  // Phase configuration
  int battPhase = _modifier->getBatteryPhase();
  doc["battery_phase"] = "L" + String(battPhase);
  
  int modPhase = _modifier->getModifyPhase();
  doc["modify_phase"] = "L" + String(modPhase);
  
  // Force power
  doc["force_power"] = _modifier->getForcePower();
  
  String payload;
  serializeJson(doc, payload);
  
  String stateTopic = getBaseTopic() + "/state";
  logPrint("[MQTT] Publishing to ");
  logPrint(stateTopic);
  logPrint(" - payload size: ");
  logPrintln(String(payload.length()));
  
  // Ensure client is still connected and responsive
  if (!_mqttClient.connected()) {
    logPrintln("[MQTT] Client disconnected before publish!");
    return;
  }
  
  // Call loop to process any pending messages
  _mqttClient.loop();
  
  // Try to publish
  bool published = _mqttClient.publish(stateTopic.c_str(), (uint8_t*)payload.c_str(), payload.length(), false);
  
  if (!published) {
    logPrint("[MQTT] Publish FAILED! State: ");
    logPrint(String(_mqttClient.state()));
    logPrint(", Connected: ");
    logPrintln(String(_mqttClient.connected()));
  } else {
    logPrintln("[MQTT] Publish success!");
  }
  
  // Keep availability topic fresh (Home Assistant heartbeat)
  String statusTopic = getBaseTopic() + "/status";
  _mqttClient.publish(statusTopic.c_str(), "online", true);
}

void HomeAssistant::handleCommand(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String payloadStr;
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  
  Serial.printf("MQTT command received: %s = %s\n", topic, payloadStr.c_str());
  
  String baseTopic = getBaseTopic() + "/cmd/";
  
  if (topicStr.startsWith(baseTopic)) {
    String command = topicStr.substring(baseTopic.length());
    
    if (command == "operation_mode") {
      if (payloadStr == "Unmodified Forward") _modifier->setMode(MODE_UNMODIFIED);
      else if (payloadStr == "Off") _modifier->setMode(MODE_OFF);
      else if (payloadStr == "Force Charge") _modifier->setMode(MODE_FORCE_CHARGE);
      else if (payloadStr == "Force Discharge") _modifier->setMode(MODE_FORCE_DISCHARGE);
      else if (payloadStr == "Charge Only") _modifier->setMode(MODE_CHARGE_ONLY);
      else if (payloadStr == "Discharge Only") _modifier->setMode(MODE_DISCHARGE_ONLY);
      
      _config->operationMode = _modifier->getMode();
      Serial.printf("Mode changed to: %s\n", payloadStr.c_str());
    }
    else if (command == "battery_phase") {
      int phase = payloadStr.substring(1).toInt(); // Remove 'L' prefix
      if (phase >= 1 && phase <= 3) {
        _modifier->setBatteryPhase(phase);
        _config->batteryPhase = phase;
        Serial.printf("Battery phase set to: L%d\n", phase);
      }
    }
    else if (command == "modify_phase") {
      int phase = payloadStr.substring(1).toInt(); // Remove 'L' prefix
      if (phase >= 1 && phase <= 3) {
        _modifier->setModifyPhase(phase);
        _config->modifyPhase = phase;
        Serial.printf("Modify phase set to: L%d\n", phase);
      }
    }
    else if (command == "force_power") {
      float power = payloadStr.toFloat();
      if (power >= 0 && power <= 20000) {
        _modifier->setForcePower(power);
        _config->forcePower = power;
        Serial.printf("Force power set to: %.1f W\n", power);
      }
    }
    else if (command == "battery_soc") {
      float soc = payloadStr.toFloat();
      if (soc >= 0.0f && soc <= 100.0f) {
        _config->batterySOC = soc;
        Serial.printf("Battery SOC set to: %.1f %%\n", soc);
      }
    }
    else if (command == "battery_power") {
      float p = payloadStr.toFloat();
      _config->batteryPower = p;
      Serial.printf("Battery power set to: %.1f W\n", p);
    }
    else if (command == "battery_capacity") {
      float cap = payloadStr.toFloat();
      if (cap >= 0.0f) {
        _config->batteryCapacity = cap;
        Serial.printf("Battery capacity set to: %.2f kWh\n", cap);
      }
    }
    else if (command == "battery_production") {
      float prod = payloadStr.toFloat();
      _config->batteryProduction = prod;
      Serial.printf("Battery production set to: %.1f W\n", prod);
    }
    else if (command == "battery_consumption") {
      float cons = payloadStr.toFloat();
      _config->batteryConsumption = cons;
      Serial.printf("Battery consumption set to: %.1f W\n", cons);
    }
    
    // Publish updated state immediately
    publishSensors();
  }
}
