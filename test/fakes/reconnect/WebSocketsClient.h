#pragma once

#include <Arduino.h>
#include <vector>

enum WStype_t {
  WStype_ERROR, WStype_DISCONNECTED, WStype_CONNECTED, WStype_TEXT,
  WStype_BIN, WStype_PING, WStype_PONG
};
enum WSCstatus_t { WSC_NOT_CONNECTED, WSC_HEADER, WSC_CONNECTED };
enum WSopcode_t { WSop_text, WSop_continuation };
struct WSclient_t { WSCstatus_t status = WSC_NOT_CONNECTED; String cUrl; };

namespace ReconnectTest {
enum class Peer { Healthy, Refuse, SilentBeforeUpgrade, SilentAfterUpgrade,
                  HttpReject, CloseFrame, HeaderTimeout, HeartbeatTimeout };
extern uint32_t now;
extern uint32_t connectDuration;
extern uint32_t cleanupDuration;
extern Peer peer;
extern std::vector<uint32_t> attempts;
extern std::vector<uint32_t> disconnects;
}

// Models the synchronous arduinoWebSockets 2.7.3 lifecycle, not network I/O.
// In particular a refused connect retains its TCP object until the next
// clientIsConnected() cleanup, which stamps _lastConnectionFail again.
class WebSocketsClient {
public:
  void enableHeartbeat(uint32_t interval, uint32_t timeout, uint8_t) {
    heartbeatInterval = interval;
    heartbeatTimeout = timeout;
  }
  void disableHeartbeat() { heartbeatInterval = 0; }
  void setReconnectInterval(unsigned long value) { _reconnectInterval = value; }
  void onEvent(std::function<void(WStype_t, uint8_t *, size_t)> value) { callback = value; }
  void begin(const String &, int, const String &) {
    _client.status = WSC_NOT_CONNECTED;
    _lastConnectionFail = 0;
  }
  void beginSSL(const char *, int, const char *) { begin("", 0, ""); }
  void disconnect() {
    if (clientIsConnected(&_client)) clientDisconnect();
  }
  bool clientIsConnected(WSclient_t *client) {
    if (!hasTcp) return false;
    // Both targets can still parse buffered replies after an ordinary FIN.
    if ((tcpUp || readableReply) && client->status != WSC_NOT_CONNECTED) return true;
    clientDisconnect();
    return false;
  }
  void loop() {
    using namespace ReconnectTest;
    if (!clientIsConnected(&_client)) {
      // unsigned long is 32-bit on the embedded targets, unlike the test host.
      if (static_cast<uint32_t>(now-_lastConnectionFail) < _reconnectInterval) return;
      attempts.push_back(now);
      hasTcp = true;
      now += connectDuration;
      if (peer == Peer::Refuse) {
        _lastConnectionFail = now;
        return;
      }
      tcpUp = true;
      _client.status = WSC_HEADER;
      _lastConnectionFail = 0;
      phaseStarted = now;
      return;
    }
    if (_client.status == WSC_HEADER) {
      if (peer == Peer::HttpReject) {
        clientDisconnect();
      } else if (peer == Peer::HeaderTimeout) {
        if (static_cast<uint32_t>(now-phaseStarted) > 5000) clientDisconnect();
      } else {
        _client.status = WSC_CONNECTED;
        phaseStarted = now;
        if (callback) callback(WStype_CONNECTED, nullptr, 0);
      }
    } else if (peer == Peer::CloseFrame && readableReply) {
      clientDisconnect();
    } else if (peer == Peer::HeartbeatTimeout && heartbeatInterval &&
               static_cast<uint32_t>(now-phaseStarted) > heartbeatInterval+heartbeatTimeout) {
      clientDisconnect();
    }
  }
  bool sendFrame(WSclient_t *, WSopcode_t, uint8_t *, size_t, bool, bool) { return true; }

protected:
  void advancePeer() {
    using namespace ReconnectTest;
    if (!tcpUp) return;
    if ((_client.status == WSC_HEADER && peer == Peer::SilentBeforeUpgrade) ||
        (_client.status == WSC_CONNECTED && peer == Peer::SilentAfterUpgrade)) {
      tcpUp = false;
    } else if ((_client.status == WSC_HEADER && peer == Peer::HttpReject) ||
               (_client.status == WSC_CONNECTED && peer == Peer::CloseFrame)) {
      tcpUp = false;
      readableReply = true;
    }
  }
  void clientDisconnect() {
    using namespace ReconnectTest;
    bool event = hasTcp;
    hasTcp = tcpUp = readableReply = false;
    _client.status = WSC_NOT_CONNECTED;
    now += cleanupDuration;
    _lastConnectionFail = now;
    if (event) {
      disconnects.push_back(now);
      if (callback) callback(WStype_DISCONNECTED, nullptr, 0);
    }
  }
  WSclient_t _client;
  // Retain the dependency's declared types to expose host-width mistakes.
  unsigned long _lastConnectionFail = 0;
  unsigned long _reconnectInterval = 500;
  bool hasTcp = false;
  bool tcpUp = false;
  bool readableReply = false;
  uint32_t phaseStarted = 0;
  uint32_t heartbeatInterval = 0;
  uint32_t heartbeatTimeout = 0;
  std::function<void(WStype_t, uint8_t *, size_t)> callback;
};
