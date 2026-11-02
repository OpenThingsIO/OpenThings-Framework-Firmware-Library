#include "Websocket.h"

#if defined(ESP32)
#include <esp_random.h>
#elif !defined(ARDUINO)
#include <sys/random.h>
#include <time.h>
#include <unistd.h>
#endif

namespace {
uint32_t reconnectSeed() {
#if defined(ESP8266)
  return uint32_t(secureRandom(0x7fffffffL)) ^ (uint32_t(secureRandom(0x7fffffffL)) << 1) ^ ESP.getChipId();
#elif defined(ESP32)
  const uint64_t mac = ESP.getEfuseMac();
  return esp_random() ^ uint32_t(mac) ^ uint32_t(mac >> 32);
#else
  uint32_t seed;
  if (getrandom(&seed, sizeof(seed), GRND_NONBLOCK) == sizeof(seed)) return seed;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return uint32_t(ts.tv_nsec) ^ uint32_t(ts.tv_sec) ^ uint32_t(getpid());
#endif
}
struct PollGuard {
  bool &flag;
  explicit PollGuard(bool &value) : flag(value) { flag = true; }
  ~PollGuard() { flag = false; }
};
}

#if defined(ARDUINO)
void WebsocketClient::enableHeartbeat(uint32_t interval, uint32_t timeout, uint8_t maxMissed) {
  WebSocketsClient::enableHeartbeat(interval, timeout, maxMissed);
}

void WebsocketClient::disableHeartbeat() {
  WebSocketsClient::disableHeartbeat();
}

void WebsocketClient::setReconnectInterval(uint32_t interval) {
  reconnectInterval = interval;
  retry.configure(millis(), reconnectSeed(), interval);
  // Only poll() may authorize a new attempt, never the transport's inner loop.
  WebSocketsClient::setReconnectInterval(UINT32_MAX);
}

void WebsocketClient::resetStreaming() {
  isStreaming = false;
}

void WebsocketClient::poll() {
  if (polling) return;
  PollGuard guard(polling);
  clientIsConnected(&_client);
  if (_client.status == WSC_NOT_CONNECTED) {
    if (attemptActive) {
      attemptActive = false;
      retry.failed(millis());
    }
    if (!enableReconnect || !retry.ready(millis())) return;
    attemptActive = true;
    WebSocketsClient::setReconnectInterval(0);
    WebSocketsClient::loop();
    WebSocketsClient::setReconnectInterval(UINT32_MAX);
  } else {
    // If TCP dies between our check and loop(), its reconnect path stays gated.
    _lastConnectionFail = millis();
    WebSocketsClient::loop();
  }
  clientIsConnected(&_client); // Free failed sockets before starting the delay.
  if (_client.status == WSC_NOT_CONNECTED && attemptActive) {
    attemptActive = false;
    retry.failed(millis());
  }
  if (_client.status == WSC_CONNECTED) retry.update(millis());
}

void WebsocketClient::onEvent(WebSocketEventCallback callback) {
  WS_DEBUG("Setting event callback\n");
  this->eventCallback = callback;
}

void WebsocketClient::connect(WSInterfaceString host, int port, WSInterfaceString path) {
  WS_DEBUG("Connecting to ws://%s:%d%s\n", host.c_str(), port, path.c_str());
  close(false);
  WebSocketsClient::begin(host, port, path);
  enableReconnect = true;
  attemptActive = false;
  setReconnectInterval(reconnectInterval);
}

void WebsocketClient::connectSecure(WSInterfaceString host, int port, WSInterfaceString path) {
  close(false);
  WebSocketsClient::beginSSL(host.c_str(), port, path.c_str());
  enableReconnect = true;
  attemptActive = false;
  setReconnectInterval(reconnectInterval);
}

bool WebsocketClient::stream() {
  if (isStreaming) {
    WS_DEBUG("Already streaming\n");
    return false;
  }

  if (clientIsConnected(&_client)) {
    isStreaming = sendFrame(&_client, WSop_text, (uint8_t *) "", 0, false, false);
  } else {
    isStreaming = false;
  }

  return isStreaming;
}

bool WebsocketClient::send(uint8_t *payload, size_t length, bool headerToPayload) {
  WS_DEBUG("Sending message of length %d\n", length);

  if (length == 0) {
    length = strlen((const char *) payload);
  }

  if (clientIsConnected(&_client)) {
    if (isStreaming) {
      return sendFrame(&_client, WSop_continuation, payload, length, false, headerToPayload);
    } else {
      return sendFrame(&_client, WSop_text, payload, length, true, headerToPayload);
    }
  }

  return false;
}

bool WebsocketClient::send(const char *payload, size_t length, bool headerToPayload) {
  return send((uint8_t *) payload, length, headerToPayload);
}

bool WebsocketClient::end() {
  if (!isStreaming) {
    return true;
  }

  WS_DEBUG("Ending stream\n");

  bool res = sendFrame(&_client, WSop_continuation, (uint8_t *) "", 0, true, false);
  isStreaming = !res;
  return res;
}

#else

void WebsocketClient::enableHeartbeat(uint32_t interval, uint32_t timeout, uint8_t maxMissed) {
    heartbeatEnabled = true;
    heartbeatInterval = interval;
    heartbeatTimeout = timeout;
    heartbeatMaxMissed = maxMissed;
}

void WebsocketClient::disableHeartbeat() {
    heartbeatEnabled = false;
}

void WebsocketClient::setReconnectInterval(uint32_t interval) {
    reconnectInterval = interval;
    retry.configure(millis(), reconnectSeed(), interval);
}

void WebsocketClient::resetStreaming() {
    isStreaming = false;
}

// Monotonic time with Arduino's 32-bit wrap semantics; wall-clock corrections
// must not change retry or heartbeat deadlines.
uint32_t millis() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return uint32_t(uint64_t(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000);
}


void WebsocketClient::poll() {
  if (polling) return;
  PollGuard guard(polling);
  websockets::WebsocketsClient::poll();
  if (heartbeatEnabled && available()) {
    if (!heartbeatInProgress && (millis() - heartbeatLastSent > heartbeatInterval)) {
      if (heartbeatMissed >= heartbeatMaxMissed) {
        // Too many missed heartbeats, close the connection
        WS_DEBUG("Too many missed heartbeats, closing connection\n");
        heartbeatMissed = 0;
        websockets::WebsocketsClient::close();
        heartbeatInProgress = false;
        return;
      }

      WS_DEBUG("Sending ping\n");
      ping();
      heartbeatLastSent = millis();
      heartbeatInProgress = true;
    }

    if (heartbeatInProgress && (millis() - heartbeatLastSent > heartbeatTimeout)) {
      // Heartbeat timeout
      WS_DEBUG("Heartbeat timeout\n");
      heartbeatMissed++;
      heartbeatInProgress = false;
      return;
    }
  }

  if (available()) {
    retry.update(millis());
    return;
  }
  if (attemptActive) {
    attemptActive = false;
    retry.failed(millis());
  }
  if (shouldReconnect && retry.ready(millis())) {
    attemptActive = true;
    if (!websockets::WebsocketsClient::connect(host, port, path)) {
      attemptActive = false;
      retry.failed(millis());
    }
  }
}

void WebsocketClient::onEvent(WebSocketEventCallback callback) {
  WS_DEBUG("Setting event callback\n");
  this->eventCallback = callback;
}

void WebsocketClient::connect(WSInterfaceString host, int port, WSInterfaceString path) {
  WS_DEBUG("Connecting to ws://%s:%d%s\n", host.c_str(), port, path.c_str());
  close(false);
  this->host = host;
  this->port = port;
  this->path = path;
  shouldReconnect = true;
  heartbeatMissed = 0;
  heartbeatInProgress = false;
//   isSecure = false;
  attemptActive = false;
  setReconnectInterval(reconnectInterval);
}

// void WebsocketClient::connectSecure(WSInterfaceString host, int port, WSInterfaceString path) {
//   WS_DEBUG("Connecting to wss://%s:%d%s\n", host.c_str(), port, path.c_str());
//   this->host = host;
//   this->port = port;
//   this->path = path;
//   shouldReconnect = true;
//   heartbeatMissed = 0;
//   heartbeatInProgress = false;
//   isSecure = true;
//   websockets::WebsocketsClient::connect(host, port, path);
// }

bool WebsocketClient::stream() {
  return websockets::WebsocketsClient::stream();
}

bool WebsocketClient::send(uint8_t *payload, size_t length, bool headerToPayload) {
    return send((const char*) payload, length, headerToPayload);
}

bool WebsocketClient::send(const char *payload, size_t length, bool headerToPayload) {
    WS_DEBUG("Sending message of length %d\n", length);
    if (length == 0) {
        length = strlen(payload);
    }

  return websockets::WebsocketsClient::send((const char*) payload, length);
}

bool WebsocketClient::end() {
  return websockets::WebsocketsClient::end();
}

#endif
