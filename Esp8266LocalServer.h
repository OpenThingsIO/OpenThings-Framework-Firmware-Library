#if defined(ESP8266)
#ifndef OTF_ESP8266LOCALSERVER_H
#define OTF_ESP8266LOCALSERVER_H

#include "LocalServer.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace OTF {
  class Esp8266LocalClient : public LocalClient {
    friend class Esp8266LocalServer;

  private:
    WiFiClient client;
    Esp8266LocalClient(WiFiClient client);

  public:
    bool dataAvailable();
    size_t readBytes(char *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    void print(const char *data);
    void print(const __FlashStringHelper *data);
    int peek();
    void setTimeout(int timeout);
    void flush();
    void stop();
  };


  class Esp8266LocalServer : public LocalServer {
  private:
    WiFiServer server;
    Esp8266LocalClient *activeClient = nullptr;

  public:
    Esp8266LocalServer(uint16_t port);

    LocalClient *acceptClient();
    void begin();
  };
}// namespace OTF

#endif
#endif
