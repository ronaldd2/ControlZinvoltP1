/*
 * TCPServer.h - TCP server for P1 telegram streaming
 */

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

// TCP port for P1 data streaming
#define P1_TCP_PORT 9988

// External references
extern WiFiServer tcpServer;
extern std::vector<WiFiClient> tcpClients;

// Logging functions from main.cpp
void logPrint(const String& msg);
void logPrintln(const String& msg);

// TCP server functions
void setupTCPServer();
void acceptTCPClients();
void broadcastP1Data(const String& telegram);
size_t getTCPClientCount();

#endif // TCPSERVER_H
