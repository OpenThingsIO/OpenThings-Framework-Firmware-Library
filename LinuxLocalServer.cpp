#if !defined(ARDUINO)
#include "LinuxLocalServer.h"

using namespace OTF;

LinuxLocalServer::LinuxLocalServer(uint16_t port) : server(port) {}

LocalClient *LinuxLocalServer::acceptClient() {
  if (activeClient != nullptr) {
    delete activeClient;
  }

  WiFiClient wiFiClient = server.available();
  if (wiFiClient) {
    activeClient = new LinuxLocalClient(wiFiClient);
  } else {
    activeClient = nullptr;
  }
  return activeClient;
}

void LinuxLocalServer::begin() {
  server.begin();
}


LinuxLocalClient::LinuxLocalClient(WiFiClient client) {
  this->client = client;
}

bool LinuxLocalClient::dataAvailable() {
  return client.available();
}

size_t LinuxLocalClient::readBytes(char *buffer, size_t length) {
  return client.readBytes(buffer, length);
}

size_t LinuxLocalClient::readBytesUntil(char terminator, char *buffer, size_t length) {
  return client.readBytesUntil(terminator, buffer, length);
}

void LinuxLocalClient::print(const char *data) {
  client.print(data);
}

size_t LinuxLocalClient::write(const char *buffer, size_t length) {
  return client.write((const uint8_t *)buffer, length);
}

int LinuxLocalClient::peek() {
  return client.peek();
}

void LinuxLocalClient::setTimeout(int timeout) {
  client.setTimeout(timeout);
}

void LinuxLocalClient::flush() {
  client.flush();
}

void LinuxLocalClient::stop() {
  client.stop();
}

#endif
