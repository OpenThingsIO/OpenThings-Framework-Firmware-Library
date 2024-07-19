#include "Websocket.h"

#if defined(ARDUINO)
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

void WebsocketClient::enableHeartbeat(unsigned long interval, unsigned long timeout, uint8_t maxMissed) {
    heartbeatEnabled = true;
    heartbeatInterval = interval;
    heartbeatTimeout = timeout;
    heartbeatMaxMissed = maxMissed;
}

void WebsocketClient::disableHeartbeat() {
    heartbeatEnabled = false;
}

void WebsocketClient::setReconnectInterval(unsigned long interval) {
    reconnectInterval = interval;
}

unsigned long millis() {
	struct timeval tv ;
	uint64_t now ;

	gettimeofday(&tv, NULL) ;
	return now  = (uint64_t)tv.tv_sec * (uint64_t)1000 + (uint64_t)(tv.tv_usec / 1000) ;
}

void WebsocketClient::poll() {
  websockets::WebsocketsClient::poll();
  if (heartbeatEnabled && available()) {
    if (!heartbeatInProgress && (millis() - heartbeatLastSent > heartbeatInterval)) {
      if (heartbeatMissed >= heartbeatMaxMissed) {
        // Too many missed heartbeats, close the connection
        WS_DEBUG("Too many missed heartbeats, closing connection\n");
        reconnectLastAttempt = 0;
        heartbeatMissed = 0;
        websockets::WebsocketsClient::close();
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

  if (shouldReconnect && !available()) {
    if (millis() - reconnectLastAttempt > reconnectInterval) {
      WS_DEBUG("Reconnecting...\n");
      // Attempt to reconnect
        websockets::WebsocketsClient::connect(host, port, path);

      WS_DEBUG("Reconnect attempt complete\n");
      WS_DEBUG("Connection status: %d\n", websockets::WebsocketsClient::available());
      reconnectLastAttempt = millis();
    }
  }
}

void WebsocketClient::onEvent(WebSocketEventCallback callback) {
  WS_DEBUG("Setting event callback\n");
  this->eventCallback = callback;
}

void WebsocketClient::connect(WSInterfaceString host, int port, WSInterfaceString path) {
  WS_DEBUG("Connecting to ws://%s:%d%s\n", host, port, path);
  this->host = host;
  this->port = port;
  this->path = path;
  shouldReconnect = true;
  heartbeatMissed = 0;
  heartbeatInProgress = false;
//   isSecure = false;
  websockets::WebsocketsClient::connect(host, port, path);
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