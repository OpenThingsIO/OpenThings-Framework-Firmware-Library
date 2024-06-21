#if defined(ESP32)
#ifndef OTF_ESP32LOCALSERVER_H
#define OTF_ESP32LOCALSERVER_H

#include "LocalServer.h"

#include <Arduino.h>
#include <WiFi.h>

namespace OTF {
  class Esp32LocalClient : public LocalClient {
    friend class Esp32LocalServer;

  private:
    WiFiClient client;
    Esp32LocalClient(WiFiClient client);

  public:
    bool dataAvailable();
    size_t readBytes(char *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    void print(const char *data);
    void print(const __FlashStringHelper *data);
    size_t write(const char *buffer, size_t length);
    size_t write(const __FlashStringHelper *buffer, size_t length);
    int peek();
    void setTimeout(int timeout);
    void flush();
    void stop();
  };


  class Esp32LocalServer : public LocalServer {
  private:
    WiFiServer server;
    Esp32LocalClient *activeClient = nullptr;

  public:
    Esp32LocalServer(uint16_t port);

    LocalClient *acceptClient();
    void begin();
  };
}// namespace OTF

#endif
#endif
