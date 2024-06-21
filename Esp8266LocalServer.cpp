#if defined(ESP8266)
#include "Esp8266LocalServer.h"

using namespace OTF;

Esp8266LocalServer::Esp8266LocalServer(uint16_t port) : server(port) {}

LocalClient *Esp8266LocalServer::acceptClient() {
  if (activeClient != nullptr) {
    delete activeClient;
  }

  WiFiClient wiFiClient = server.available();
  if (wiFiClient) {
    activeClient = new Esp8266LocalClient(wiFiClient);
  } else {
    activeClient = nullptr;
  }
  return activeClient;
}

void Esp8266LocalServer::begin() {
  server.begin();
}


Esp8266LocalClient::Esp8266LocalClient(WiFiClient client) {
  this->client = client;
}

bool Esp8266LocalClient::dataAvailable() {
  return client.available();
}

size_t Esp8266LocalClient::readBytes(char *buffer, size_t length) {
  return client.readBytes(buffer, length);
}

size_t Esp8266LocalClient::readBytesUntil(char terminator, char *buffer, size_t length) {
  return client.readBytesUntil(terminator, buffer, length);
}

void Esp8266LocalClient::print(const char *data) {
  client.print(data);
}

void Esp8266LocalClient::print(const __FlashStringHelper *data) {
  client.print(data);
}

size_t Esp8266LocalClient::write(const char *buffer, size_t size) {
  return client.write((const uint8_t *)buffer, size);
}

size_t Esp8266LocalClient::write(const __FlashStringHelper *buffer, size_t size) {
  return client.write_P((const char*) buffer, size);
}

int Esp8266LocalClient::peek() {
  return client.peek();
}

void Esp8266LocalClient::setTimeout(int timeout) {
  client.setTimeout(timeout);
}

void Esp8266LocalClient::flush() {
	client.flush();
}

void Esp8266LocalClient::stop() {
	client.stop();
}

#endif
