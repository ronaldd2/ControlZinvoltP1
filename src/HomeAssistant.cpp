/*
 * HomeAssistant.cpp - Implementation of MQTT Home Assistant Integration
 */

#include "HomeAssistant.h"

HomeAssistant::HomeAssistant(P1Parser* parser, P1Modifier* modifier, Config* config) {
  parser_ = parser;
  modifier_ = modifier;
  config_ = config;
  mqtt_client_.setClient(wifi_client_);
  last_reconnect_ = 0;
  last_publish_ = 0;
  last_discovery_ = 0;
  mqtt_port_ = 1883;
  publish_requested_ = false;
  
  // Generate unique device ID from MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  device_id_ = "controlzinvoltp1_" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
}

void HomeAssistant::begin(const char* mqttServer, int mqttPort, 
                           const char* mqttUser, const char* mqttPassword) {
  setMqttConfig(mqttServer, mqttPort, mqttUser, mqttPassword);
  
  mqtt_client_.setServer(mqttServer, mqttPort);
  mqtt_client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->handleCommand(topic, payload, length);
  });
  
  Serial.println("Home Assistant MQTT initialized");
  Serial.printf("  Server: %s:%d\n", mqttServer, mqttPort);
  Serial.printf("  Device ID: %s\n", device_id_.c_str());
}

void HomeAssistant::setMqttConfig(const char* server, int port, const char* user, const char* pass) {
  mqtt_server_ = server ? server : "";
  mqtt_port_ = port;
  mqtt_user_ = user ? user : "";
  mqtt_password_ = pass ? pass : "";
}

void HomeAssistant::reconnectNow() {
  if (mqtt_client_.connected()) {
    mqtt_client_.disconnect();
  }
  last_reconnect_ = 0; // Force immediate reconnect
}

void HomeAssistant::loop() {
  static unsigned long lastConnCheck = 0;
  unsigned long now = millis();
  
  // Check connection status more frequently
  if (now - lastConnCheck > 1000) {
    lastConnCheck = now;
    // Force PubSubClient to check socket status
    if (mqtt_client_.connected()) {
      mqtt_client_.loop();  // This will detect broken connections
    }
  }
  
  if (!mqtt_client_.connected()) {
    if (now - last_reconnect_ > 5000) {
      last_reconnect_ = now;
      logPrintln("[MQTT] Reconnecting...");
      reconnect();
    }
  } else {
    // Publish only when a new P1 telegram was processed
    if (publish_requested_ && (now - last_publish_ > 200)) { // throttle to avoid bursts
      publish_requested_ = false;
      last_publish_ = now;
      publishSensors();
    }
    
    // Republish discovery every 5 minutes
    if (now - last_discovery_ > 300000) {
      last_discovery_ = now;
      publishDiscovery();
    }
  }
}

void HomeAssistant::requestPublish() {
  publish_requested_ = true;
}

bool HomeAssistant::isConnected() {
  return mqtt_client_.connected();
}

void HomeAssistant::reconnect() {
  if (mqtt_server_.isEmpty()) {
    Serial.println("[MQTT] Server not configured!");
    return;
  }
  
  Serial.print("[MQTT] Connecting to ");
  Serial.print(mqtt_server_);
  Serial.print(":");
  Serial.println(mqtt_port_);
  
  String clientId = device_id_ + "_" + String(random(0xffff), HEX);
  
  bool connected;
  if (mqtt_user_.isEmpty()) {
    connected = mqtt_client_.connect(clientId.c_str());
  } else {
    connected = mqtt_client_.connect(clientId.c_str(), mqtt_user_.c_str(), mqtt_password_.c_str());
  }
  
  if (connected) {
    Serial.println("[MQTT] Connected!");
    
    // Publish availability (online)
    String statusTopic = getBaseTopic() + "/status";
    mqtt_client_.publish(statusTopic.c_str(), "online", true);
    Serial.println("[MQTT] Published availability: online");
    
    String cmdTopic = getBaseTopic() + "/cmd/#";
    mqtt_client_.subscribe(cmdTopic.c_str());
    publishDiscovery();
    publishSensors();
  } else {
    Serial.print("[MQTT] Failed, rc=");
    Serial.println(mqtt_client_.state());
  }
}

String HomeAssistant::getBaseTopic() {
  return "controlzinvoltp1/" + device_id_;
}

String HomeAssistant::getDiscoveryTopic(const char* component, const char* objectId) {
  return "homeassistant/" + String(component) + "/" + device_id_ + "/" + String(objectId) + "/config";
}

void HomeAssistant::publishDiscovery() {
  Serial.println("Publishing Home Assistant discovery messages...");
  
  // Device information (common for all entities)
  JsonDocument deviceDoc;
  JsonArray identifiers = deviceDoc["identifiers"].to<JsonArray>();
  identifiers.add(device_id_);
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
  publishSensor("Grid Power", "grid_power", NULL, "W", "mdi:transmission-tower");
  publishNumber("Battery Capacity", "battery_capacity", "mdi:battery-high", 0, 100, 0.1, "kWh");
  publishNumber("Battery Production", "battery_production", "mdi:solar-power", 0, 50000, 1, "W");
  publishNumber("Battery Consumption", "battery_consumption", "mdi:transmission-tower", 0, 50000, 1, "W");
  publishNumber("Solar Power Input", "solar_power", "mdi:solar-power", 0, 20000, 1, "W");
  
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
  doc["unique_id"] = device_id_ + "_" + String(objectId);
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
  identifiers.add(device_id_);
  device["name"] = "ControlZinvolt P1";
  device["model"] = "ESP32-S3 P1 Controller";
  device["manufacturer"] = "AI";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("sensor", objectId);
  mqtt_client_.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishBinarySensor(const char* name, const char* objectId, const char* deviceClass) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = device_id_ + "_" + String(objectId);
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
  identifiers.add(device_id_);
  device["name"] = "ControlZinvolt P1";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("binary_sensor", objectId);
  mqtt_client_.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishSelect(const char* name, const char* objectId, const char* icon) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = device_id_ + "_" + String(objectId);
  doc["state_topic"] = getBaseTopic() + "/state";
  doc["command_topic"] = getBaseTopic() + "/cmd/" + String(objectId);
  
  if (strcmp(objectId, "operation_mode") == 0) {
    doc["value_template"] = "{{ value_json.operation_mode }}";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("Unmodified Forward");
    options.add("Off");
    options.add("Force Charge");
    options.add("Force Discharge");
    options.add("Power Control");
    options.add("Charge Only");
    options.add("Discharge Only");
    options.add("External Control");
    options.add("Optimize");
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
  identifiers.add(device_id_);
  device["name"] = "ControlZinvolt P1";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("select", objectId);
  mqtt_client_.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishNumber(const char* name, const char* objectId, const char* icon,
                                   float min, float max, float step, const char* unit) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = device_id_ + "_" + String(objectId);
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
  identifiers.add(device_id_);
  device["name"] = "ControlZinvolt P1";
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = getDiscoveryTopic("number", objectId);
  mqtt_client_.publish(topic.c_str(), payload.c_str(), true);
}

void HomeAssistant::publishSensors() {
  if (!mqtt_client_.connected()) {
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
  float l1Power = parser_->getActivePowerL1();
  float l2Power = parser_->getActivePowerL2();
  float l3Power = parser_->getActivePowerL3();
  float totalPower = l1Power + l2Power + l3Power;
  float l1Current = parser_->getCurrentL1();
  float l2Current = parser_->getCurrentL2();
  float l3Current = parser_->getCurrentL3();
  float v1 = parser_->getVoltageL1();
  float v2 = parser_->getVoltageL2();
  float v3 = parser_->getVoltageL3();
  
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
  doc["energy_import"] = parser_->getTotalEnergyImport();
  doc["energy_export"] = parser_->getTotalEnergyExport();

  // Daily energy (relative to start of day baselines)
  float dailyImport = 0.0f;
  float dailyExport = 0.0f;
  if (config_->dayStartEnergyImport > 0.0f) {
    dailyImport = max(0.0f, parser_->getTotalEnergyImport() - config_->dayStartEnergyImport);
  }
  if (config_->dayStartEnergyExport > 0.0f) {
    dailyExport = max(0.0f, parser_->getTotalEnergyExport() - config_->dayStartEnergyExport);
  }
  doc["daily_energy_import"] = dailyImport;
  doc["daily_energy_export"] = dailyExport;
  
  // System data
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  
  // P1 valid status (send as string to match binary_sensor payload_on/payload_off)
  doc["p1_valid"] = parser_->isValid() ? "true" : "false";
  
  // Battery data (from config)
  doc["battery_soc"] = config_->batterySOC;
  doc["battery_power"] = config_->batteryPower;
  doc["grid_power"] = config_->gridPower;
  doc["battery_capacity"] = config_->batteryCapacity;
  doc["battery_production"] = config_->batteryProduction;
  doc["battery_consumption"] = config_->batteryConsumption;
  
  // Operation mode
  String modeStr = modifier_->getModeString();
  doc["operation_mode"] = modeStr;
  
  // Phase configuration
  int battPhase = modifier_->getBatteryPhase();
  doc["battery_phase"] = "L" + String(battPhase);
  
  int modPhase = modifier_->getModifyPhase();
  doc["modify_phase"] = "L" + String(modPhase);
  
  // Force power
  doc["force_power"] = modifier_->getForcePower();
  
  String payload;
  serializeJson(doc, payload);
  
  String stateTopic = getBaseTopic() + "/state";
  logPrint("[MQTT] Publishing to ");
  logPrint(stateTopic);
  logPrint(" - payload size: ");
  logPrintln(String(payload.length()));
  
  // Ensure client is still connected and responsive
  if (!mqtt_client_.connected()) {
    logPrintln("[MQTT] Client disconnected before publish!");
    return;
  }
  
  // Call loop to process any pending messages
  mqtt_client_.loop();
  
  // Try to publish
  bool published = mqtt_client_.publish(stateTopic.c_str(), (uint8_t*)payload.c_str(), payload.length(), false);
  
  if (!published) {
    logPrint("[MQTT] Publish FAILED! State: ");
    logPrint(String(mqtt_client_.state()));
    logPrint(", Connected: ");
    logPrintln(String(mqtt_client_.connected()));
  } else {
    logPrintln("[MQTT] Publish success!");
  }
  
  // Keep availability topic fresh (Home Assistant heartbeat)
  String statusTopic = getBaseTopic() + "/status";
  mqtt_client_.publish(statusTopic.c_str(), "online", true);
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
      if (payloadStr == "Unmodified Forward") modifier_->setMode(MODE_UNMODIFIED);
      else if (payloadStr == "Off") modifier_->setMode(MODE_BATTERY_OFF);
      else if (payloadStr == "Force Charge") modifier_->setMode(MODE_FORCE_CHARGE);
      else if (payloadStr == "Force Discharge") modifier_->setMode(MODE_FORCE_DISCHARGE);
      else if (payloadStr == "Power Control") modifier_->setMode(MODE_POWER_CONTROL);
      else if (payloadStr == "Charge Only") modifier_->setMode(MODE_CHARGE_ONLY);
      else if (payloadStr == "Discharge Only") modifier_->setMode(MODE_DISCHARGE_ONLY);
      else if (payloadStr == "External Control") modifier_->setMode(MODE_EXTERNAL_CONTROL);
      else if (payloadStr == "Optimize") modifier_->setMode(MODE_OPTIMIZE);
      
      config_->operationMode = modifier_->getMode();
      Serial.printf("Mode changed to: %s\n", payloadStr.c_str());
    }
    else if (command == "battery_phase") {
      int phase = payloadStr.substring(1).toInt(); // Remove 'L' prefix
      if (phase >= 1 && phase <= 3) {
        modifier_->setBatteryPhase(phase);
        config_->batteryPhase = phase;
        Serial.printf("Battery phase set to: L%d\n", phase);
      }
    }
    else if (command == "modify_phase") {
      int phase = payloadStr.substring(1).toInt(); // Remove 'L' prefix
      if (phase >= 1 && phase <= 3) {
        modifier_->setModifyPhase(phase);
        config_->modifyPhase = phase;
        Serial.printf("Modify phase set to: L%d\n", phase);
      }
    }
    else if (command == "force_power") {
      float power = payloadStr.toFloat();
      if (power >= 0 && power <= 20000) {
        modifier_->setForcePower(power);
        config_->forcePower = power;
        Serial.printf("Force power set to: %.1f W\n", power);
      }
    }
    else if (command == "battery_soc") {
      float soc = payloadStr.toFloat();
      if (soc >= 0.0f && soc <= 100.0f) {
        config_->batterySOC = soc;
        Serial.printf("Battery SOC set to: %.1f %%\n", soc);
      }
    }
    else if (command == "battery_power") {
      float p = payloadStr.toFloat();
      config_->batteryPower = p;
      Serial.printf("Battery power set to: %.1f W\n", p);
    }
    else if (command == "battery_capacity") {
      float cap = payloadStr.toFloat();
      if (cap >= 0.0f) {
        config_->batteryCapacity = cap;
        Serial.printf("Battery capacity set to: %.2f kWh\n", cap);
      }
    }
    else if (command == "battery_production") {
      float prod = payloadStr.toFloat();
      config_->batteryProduction = prod;
      Serial.printf("Battery production set to: %.1f W\n", prod);
    }
    else if (command == "battery_consumption") {
      float cons = payloadStr.toFloat();
      config_->batteryConsumption = cons;
      Serial.printf("Battery consumption set to: %.1f W\n", cons);
    }
    else if (command == "solar_power") {
      float solar = payloadStr.toFloat();
      if (solar >= 0.0f && solar <= 20000.0f) {
        config_->actualSolarPower = solar;
        Serial.printf("Solar power set to: %.1f W\n", solar);
      }
    }
    
    // Publish updated state immediately
    publishSensors();
  }
}
