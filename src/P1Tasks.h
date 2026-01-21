/*
 * P1Tasks.h - P1 telegram reading and relay tasks
 */

#ifndef P1TASKS_H
#define P1TASKS_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include "P1Parser.h"
#include "P1Modifier.h"
#include "Config.h"
#include "TCPServer.h"

// External references from main.cpp
extern WiFiClient telnetClient;
extern std::vector<WiFiClient> tcpClients;
extern String currentP1Telegram;
extern bool telegramComplete;
extern bool telegramSent;
extern P1Parser p1Parser;
extern P1Modifier p1Modifier;
extern Config config;

// Logging functions from main.cpp
void logPrint(const String& msg);
void logPrintln(const String& msg);

// FreeRTOS tasks
void readP1Task(void* parameter);
void relayP1Task(void* parameter);

#endif // P1TASKS_H
