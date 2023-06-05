#if defined(ESP32)
#include "Esp32LocalServer.h"

using namespace OTF;

Esp32LocalServer::Esp32LocalServer(uint16_t port) : server(port) {}

LocalClient *Esp32LocalServer::acceptClient() {
  if (activeClient != nullptr) {
    delete activeClient;
  }

  WiFiClient wiFiClient = server.available();
  if (wiFiClient) {
    activeClient = new Esp32LocalClient(wiFiClient);
  } else {
    activeClient = nullptr;
  }
  return activeClient;
}

void Esp32LocalServer::begin() {
  server.begin();
}


Esp32LocalClient::Esp32LocalClient(WiFiClient client) {
  this->client = client;
}

bool Esp32LocalClient::dataAvailable() {
  return client.available();
}

size_t Esp32LocalClient::readBytes(char *buffer, size_t length) {
  return client.readBytes(buffer, length);
}

size_t Esp32LocalClient::readBytesUntil(char terminator, char *buffer, size_t length) {
  return client.readBytesUntil(terminator, buffer, length);
}

void Esp32LocalClient::print(const char *data) {
  client.print(data);
}

void Esp32LocalClient::print(const __FlashStringHelper *data) {
  client.print(data);
}

int Esp32LocalClient::peek() {
  return client.peek();
}

void Esp32LocalClient::setTimeout(int timeout) {
  client.setTimeout(timeout);
}

void Esp32LocalClient::flush() {
	client.flush();
}

void Esp32LocalClient::stop() {
	client.stop();
}
#endif
