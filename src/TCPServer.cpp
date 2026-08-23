/*
 * TCPServer.cpp - TCP server implementation for P1 streaming
 */

#include "TCPServer.h"

void setupTCPServer() {
  logPrintln("Setting up TCP servers for P1 streaming...");
  
  // Original P1 data server (sync WiFiServer)
  tcpServer.begin();
  logPrint("TCP server (original) started on port ");
  logPrintln(String(P1_TCP_PORT));
}

void acceptTCPClients() {
  // Accept new clients if pending
  if (tcpServer.hasClient()) {
    WiFiClient newClient = tcpServer.available();
    if (newClient) {
      newClient.setNoDelay(true);
      logPrint("New TCP client connected: ");
      logPrintln(newClient.remoteIP().toString());
      tcpClients.push_back(newClient);
    }
  }

  // Drop disconnected clients
  for (auto it = tcpClients.begin(); it != tcpClients.end(); ) {
    if (!it->connected()) {
      it = tcpClients.erase(it);
    } else {
      ++it;
    }
  }
}

void broadcastP1Data(const String& telegram) {
  // Broadcast to all connected TCP clients (original data)
  if (tcpClients.empty()) return;
  
  for (auto it = tcpClients.begin(); it != tcpClients.end(); ) {
    if (!it->connected()) {
      it = tcpClients.erase(it);
      continue;
    }
    size_t written = it->write((const uint8_t*)telegram.c_str(), telegram.length());
    if (written != telegram.length()) {
      it = tcpClients.erase(it);
    } else {
      ++it;
    }
  }
}

size_t getTCPClientCount() {
  return tcpClients.size();
}
