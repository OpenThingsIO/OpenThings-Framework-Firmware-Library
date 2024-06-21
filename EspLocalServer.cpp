#if defined(ESP8266) || defined(ESP32)
#include "EspLocalServer.h"

using namespace OTF;

EspLocalServer::EspLocalServer(uint16_t port) : server(port) {}

LocalClient *EspLocalServer::acceptClient() {
  if (activeClient != nullptr) {
    delete activeClient;
  }

  WiFiClient wiFiClient = server.available();
  if (wiFiClient) {
    activeClient = new EspLocalClient(wiFiClient);
  } else {
    activeClient = nullptr;
  }
  return activeClient;
}

void EspLocalServer::begin() {
  server.begin();
}


EspLocalClient::EspLocalClient(WiFiClient client) {
  this->client = client;
}

bool EspLocalClient::dataAvailable() {
  return client.available();
}

size_t EspLocalClient::readBytes(char *buffer, size_t length) {
  return client.readBytes(buffer, length);
}

size_t EspLocalClient::readBytesUntil(char terminator, char *buffer, size_t length) {
  return client.readBytesUntil(terminator, buffer, length);
}

void EspLocalClient::print(const char *data) {
  client.print(data);
}

void EspLocalClient::print(const __FlashStringHelper *data) {
  client.print(data);
}

int EspLocalClient::peek() {
  return client.peek();
}

void EspLocalClient::setTimeout(int timeout) {
  client.setTimeout(timeout);
}

void EspLocalClient::flush() {
	client.flush();
}

void EspLocalClient::stop() {
	client.stop();
}

#endif
