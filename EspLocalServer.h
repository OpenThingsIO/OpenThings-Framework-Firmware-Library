#if defined(ESP8266) || defined(ESP32)
#ifndef OTF_ESPLOCALSERVER_H
#define OTF_ESPLOCALSERVER_H

#include "LocalServer.h"

#include <Arduino.h>
#if defined(ESP8266)
	#include <ESP8266WiFi.h>
#else
	#include <WiFi.h>
#endif

namespace OTF {
  class EspLocalClient : public LocalClient {
    friend class EspLocalServer;

  private:
    WiFiClient client;
    EspLocalClient(WiFiClient client);

  public:
    bool dataAvailable();
    size_t readBytes(char *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    void print(const char *data);
    void print(const __FlashStringHelper *const data);
    size_t write(const char *buffer, size_t size);
    size_t write(const __FlashStringHelper *buffer, size_t size);
    int peek();
    void setTimeout(int timeout);
    void flush();
    void stop();
  };


  class EspLocalServer : public LocalServer {
  private:
    WiFiServer server;
    EspLocalClient *activeClient = nullptr;

  public:
    EspLocalServer(uint16_t port);

    LocalClient *acceptClient();
    void begin();
  };
}// namespace OTF

#endif
#endif
