#include "Websocket.h"

void WebsocketClient::enableHeartbeat(unsigned long interval, unsigned long timeout, uint8_t maxMissed) {
  WebSocketsClient::enableHeartbeat(interval, timeout, maxMissed);
}

void WebsocketClient::disableHeartbeat() {
  WebSocketsClient::disableHeartbeat();
}

void WebsocketClient::setReconnectInterval(unsigned long interval) {
  WebSocketsClient::setReconnectInterval(interval);
}

void WebsocketClient::poll() {
  WebSocketsClient::loop();
}

void WebsocketClient::onEvent(WebSocketEventCallback callback) {
  WS_DEBUG("Setting event callback\n");
  this->eventCallback = callback;
}

void WebsocketClient::connect(WSInterfaceString host, int port, WSInterfaceString path) {
  WS_DEBUG("Connecting to ws://%s:%d%s\n", host.c_str(), port, path.c_str());
  WebSocketsClient::begin(host, port, path);
}

void WebsocketClient::connectSecure(WSInterfaceString host, int port, WSInterfaceString path) {
  WebSocketsClient::beginSSL(host.c_str(), port, path.c_str());
}

bool WebsocketClient::stream() {
  if (isStreaming) {
    WS_DEBUG("Already streaming\n");
    return false;
  }

  if (clientIsConnected(&_client)) {
    isStreaming = sendFrame(&_client, WSop_text, (uint8_t *)"", 0, false, false);
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

  bool res = sendFrame(&_client, WSop_continuation, (uint8_t *)"", 0, true, false);
  isStreaming = !res;
  return res;
}