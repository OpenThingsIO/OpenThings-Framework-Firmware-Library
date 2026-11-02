#ifndef OTF_TEST_CLOUD_TRANSPORT_H
#define OTF_TEST_CLOUD_TRANSPORT_H

// Replace transports, not request/response or framework implementation, in
// host tests of both the Arduino deferred and Linux inline paths.
#define WEBSOCKET_H
#define OTF_ESP8266LOCALSERVER_H
#define OTF_ESP32LOCALSERVER_H
#define OTF_LINUXLOCALSERVER_H
#define WEBSOCKETS_MAX_DATA_SIZE (15 * 1024)

#include "LocalServer.h"
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <assert.h>

uint32_t millis();

namespace OTF {
class FakeLocalClient : public LocalClient {
public:
  std::string input;
  std::string output;
  size_t offset = 0;
  bool dataAvailable() override { return offset < input.size(); }
  size_t availableBytes() override { return input.size() - offset; }
  size_t readBytes(char *buffer, size_t length) override {
    length = std::min(length, availableBytes());
    memcpy(buffer, input.data() + offset, length);
    offset += length;
    return length;
  }
  size_t readBytesUntil(char terminator, char *buffer, size_t length) override {
    size_t read = 0;
    while (dataAvailable() && read < length) {
      char c = input[offset++];
      if (c == terminator) break;
      buffer[read++] = c;
    }
    return read;
  }
  size_t write(const char *data, size_t length) override {
    output.append(data, length);
    return length;
  }
  void print(const char *data) override { output += data; }
#if defined(ARDUINO)
  void print(const __FlashStringHelper *data) override {
    print(reinterpret_cast<const char *>(data));
  }
#endif
  void setTimeout(int) override {}
  void flush() override {}
  void stop() override {}
};

extern FakeLocalClient *nextLocalClient;
class FakeLocalServer : public LocalServer {
public:
  explicit FakeLocalServer(uint16_t) {}
  LocalClient *acceptClient() override {
    auto *client = nextLocalClient;
    nextLocalClient = nullptr;
    return client;
  }
  void begin() override {}
};
using Esp8266LocalServer = FakeLocalServer;
using Esp32LocalServer = FakeLocalServer;
using LinuxLocalServer = FakeLocalServer;
}

enum WSEvent_t {
  WSEvent_ERROR, WSEvent_DISCONNECTED, WSEvent_CONNECTED, WSEvent_TEXT,
  WSEvent_BIN, WSEvent_PING, WSEvent_PONG
};

class WebsocketClient;
extern WebsocketClient *testSocket;
extern bool failCloudAllocation;

class WebsocketClient {
public:
  struct Event { WSEvent_t type; std::string payload; };
  std::deque<Event> events;
  std::vector<std::string> responses;
  std::string currentResponse;
  std::function<void(WSEvent_t, uint8_t *, size_t)> callback;
  std::function<void()> afterCallback;
  std::function<void()> onSend;
  std::function<void()> onStream;
  bool streaming = false;
  bool inPoll = false;
  bool drainAll = false;
  bool failEnd = false;
  bool failSend = false;
  bool failStream = false;
  bool reconnectOnClose = false;
  unsigned closeCount = 0;
  unsigned pollCount = 0;

  WebsocketClient() { testSocket = this; }
  void onEvent(std::function<void(WSEvent_t, uint8_t *, size_t)> fn) { callback = fn; }
  template<class T> void connect(T, int, T) { emit(WSEvent_CONNECTED); }
  void setReconnectInterval(uint32_t) {}
  void enableHeartbeat(uint32_t, uint32_t, uint8_t) {}
  void resetStreaming() { streaming = false; currentResponse.clear(); }
  bool isStreamOpen() const { return streaming; }
  void emit(WSEvent_t type, const std::string &payload = "") {
    std::vector<uint8_t> bytes(payload.begin(), payload.end());
    bytes.push_back(0);
    callback(type, bytes.data(), payload.size());
    std::fill(bytes.begin(), bytes.end(), 0xdd);
  }
  void poll() {
    assert(!inPoll);
    inPoll = true;
    ++pollCount;
    while (!events.empty()) {
      Event event = events.front();
      events.pop_front();
      emit(event.type, event.payload);
      if (afterCallback) afterCallback();
      if (!drainAll) break;
    }
    inPoll = false;
  }
  bool stream() {
    if (streaming || failStream) return false;
    streaming = true;
    currentResponse.clear();
    if (onStream) onStream();
    return true;
  }
  bool send(const char *data, size_t length) {
    if (onSend) onSend();
    if (failSend) return false;
    if (streaming) currentResponse.append(data, length);
    else responses.emplace_back(data, length);
    return true;
  }
  bool end() {
    if (failEnd) return false;
    if (streaming) responses.push_back(currentResponse);
    streaming = false;
    currentResponse.clear();
    return true;
  }
  void close(bool reconnect = false) {
    assert(!inPoll);
    ++closeCount;
    reconnectOnClose = reconnect;
    emit(WSEvent_DISCONNECTED);
  }
};

#endif
