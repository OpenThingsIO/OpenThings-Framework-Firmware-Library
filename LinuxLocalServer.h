#if !defined(ARDUINO)
#ifndef OTF_LINUXLOCALSERVER_H
#define OTF_LINUXLOCALSERVER_H

#include "LocalServer.h"

namespace OTF {
  class LinuxLocalClient : public LocalClient {
    friend class Esp32LocalServer;

  private:
    int client;
    LinuxLocalClient(int client);

  public:
    bool dataAvailable();
    size_t readBytes(char *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    void print(const char *data);
    size_t write(const char *buffer, size_t length);
    int peek();
    void setTimeout(int timeout);
    void flush();
    void stop();
  };


  class LinuxLocalServer : public LocalServer {
  private:
    int server;
    LinuxLocalServer *activeClient = nullptr;

  public:
    LinuxLocalServer(uint16_t port);

    LocalClient *acceptClient();
    void begin();
  };
}// namespace OTF

#endif
#endif
