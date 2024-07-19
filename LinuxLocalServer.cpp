#if !defined(ARDUINO)
#include "LinuxLocalServer.h"

using namespace OTF;

LinuxLocalServer::LinuxLocalServer(uint16_t port) : server(port) {}

LinuxLocalServer::~LinuxLocalServer() {
  if (activeClient != nullptr)
    activeClient->stop();
    delete activeClient;
}


LocalClient *LinuxLocalServer::acceptClient() {
  if (activeClient != nullptr) {
    activeClient->stop();
    delete activeClient;
  }

  EthernetClient client = server.available();
  if (client) {
    activeClient = new LinuxLocalClient(client);
  } else {
    activeClient = nullptr;
  }
  return activeClient;
}


void LinuxLocalServer::begin() {
  server.begin();
}


LinuxLocalClient::LinuxLocalClient(EthernetClient client) {
  this->client = client;
}

bool LinuxLocalClient::dataAvailable() {
  return client.available();
}

size_t LinuxLocalClient::readBytes(char *buffer, size_t length) {
  return client.read((uint8_t*) buffer, length);
}

size_t LinuxLocalClient::readBytesUntil(char terminator, char *buffer, size_t length) {
    uint8_t buf[1] = {0};
    int res = client.read(buf, 1);
    char c = (char) buf[0];
    size_t n = 0;

    while (res >= 0 && c != terminator && length > 0) {
        buffer[n] = c;
        length--;
        n++;
        res = client.read(buf, 1);
        c = (char) buf[0];
    }

    return n;
}

void LinuxLocalClient::print(const char *data) {
  client.write((uint8_t*)data, strlen(data));
}

size_t LinuxLocalClient::write(const char *buffer, size_t size) {
  return client.write((uint8_t*)buffer, size);
}

/*int LinuxLocalClient::peek() {
  return client.peek();
}*/

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