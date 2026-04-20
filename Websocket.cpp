#include "Websocket.h"

#if defined(ARDUINO)
void WebsocketClient::beginStoredConnection() {
  if (host.length() == 0 || port <= 0 || path.length() == 0) {
    return;
  }

  if (reconnectBackoffInterval < reconnectInterval) {
    reconnectBackoffInterval = reconnectInterval;
  }

  lastConnectAttempt = millis();
  nextConnectAt = lastConnectAttempt + reconnectBackoffInterval;

  if (isSecure) {
    WS_DEBUG("Connecting to wss://%s:%d%s (deferred)\n", host.c_str(), port, path.c_str());
    WebSocketsClient::beginSSL(host.c_str(), port, path.c_str());
  } else {
    WS_DEBUG("Connecting to ws://%s:%d%s (deferred)\n", host.c_str(), port, path.c_str());
    WebSocketsClient::begin(host.c_str(), port, path.c_str());
  }
}

void WebsocketClient::enableHeartbeat(unsigned long interval, unsigned long timeout, uint8_t maxMissed) {
  WebSocketsClient::enableHeartbeat(interval, timeout, maxMissed);
}

void WebsocketClient::disableHeartbeat() {
  WebSocketsClient::disableHeartbeat();
}

void WebsocketClient::setReconnectInterval(unsigned long interval) {
#if defined(ESP8266)
  enableReconnect = true;
  reconnectInterval = interval;
  reconnectBackoffInterval = interval;
  if (nextConnectAt == 0) {
    nextConnectAt = millis() + interval;
  }
#else
  WebSocketsClient::setReconnectInterval(interval);
#endif
}

void WebsocketClient::poll() {
#if defined(ESP8266)
  if (enableReconnect && host.length() > 0 && port > 0 && path.length() > 0 && !clientIsConnected(&_client)) {
    if ((long)(millis() - nextConnectAt) >= 0) {
      beginStoredConnection();
    }
  }
  yield();
#endif
  WebSocketsClient::loop();
#if defined(ESP8266)
  yield();
#endif
}

void WebsocketClient::onEvent(WebSocketEventCallback callback) {
  WS_DEBUG("Setting event callback\n");
  this->eventCallback = callback;
}

void WebsocketClient::connect(WSInterfaceString host, int port, WSInterfaceString path) {
  this->host = host;
  this->port = port;
  this->path = path;
  this->isSecure = false;

#if defined(ESP8266)
  enableReconnect = true;
  if (reconnectInterval == 0) {
    reconnectInterval = 30000UL;
  }
  reconnectBackoffInterval = reconnectInterval;
  nextConnectAt = millis() + WS_ESP8266_INITIAL_CONNECT_DELAY;
  lastConnectAttempt = 0;
  WS_DEBUG("Scheduling ws://%s:%d%s after boot delay\n", host.c_str(), port, path.c_str());
#else
  WS_DEBUG("Connecting to ws://%s:%d%s\n", host.c_str(), port, path.c_str());
  WebSocketsClient::begin(host, port, path);
#endif
}

void WebsocketClient::connectSecure(WSInterfaceString host, int port, WSInterfaceString path) {
  this->host = host;
  this->port = port;
  this->path = path;
  this->isSecure = true;

#if defined(ESP8266)
  enableReconnect = true;
  if (reconnectInterval == 0) {
    reconnectInterval = 30000UL;
  }
  reconnectBackoffInterval = reconnectInterval;
  nextConnectAt = millis() + WS_ESP8266_INITIAL_CONNECT_DELAY;
  lastConnectAttempt = 0;
  WS_DEBUG("Scheduling wss://%s:%d%s after boot delay\n", host.c_str(), port, path.c_str());
#else
  WS_DEBUG("Connecting to wss://%s:%d%s (insecure mode)\n", host.c_str(), port, path.c_str());
  
  // For ESP32: Set SSL to insecure mode to avoid certificate validation failures
  // This is necessary because we don't have CA certificates configured
  #if defined(ESP32)
    // Begin SSL connection (this sets _client.isSSL = true internally)
    WebSocketsClient::beginSSL(host.c_str(), port, path.c_str());
    
    // Note: arduinoWebSockets library will create WiFiClientSecure internally
    // and call setInsecure() is done via the SSL_AXTLS mode which doesn't validate certs
    // However, for better memory management on ESP32-C5, we rely on the library's
    // default insecure behavior when no fingerprint/CA is provided
  #else
    WebSocketsClient::beginSSL(host.c_str(), port, path.c_str());
  #endif
#endif
}

void WebsocketClient::resetStreaming() {
    isStreaming = false;
}

bool WebsocketClient::stream() {
  if (clientIsConnected(&_client)) {
    if (isStreaming) {
      WS_DEBUG("Already streaming\n");
      return false;
    }
    isStreaming = sendFrame(&_client, WSop_text, (uint8_t *) "", 0, false, false);
  } else {
    WS_DEBUG("Client is not connected\n");
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
      bool result = sendFrame(&_client, WSop_continuation, payload, length, false, headerToPayload);
      return result;
    } else {
      bool result = sendFrame(&_client, WSop_text, payload, length, true, headerToPayload);
      return result;
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
  struct timeval tv;
  uint64_t now;

  gettimeofday(&tv, NULL);
  return now = (uint64_t) tv.tv_sec * (uint64_t) 1000 + (uint64_t) (tv.tv_usec / 1000);
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
  websockets::WebsocketsClient::connect(this->host.c_str(), this->port, this->path.c_str());
}

void WebsocketClient::connectSecure(WSInterfaceString host, int port, WSInterfaceString path) {
  WS_DEBUG("Connecting to wss://%s:%d%s\n", host.c_str(), port, path.c_str());
  this->host = host;
  this->port = port;
  this->path = path;
  shouldReconnect = true;
  heartbeatMissed = 0;
  heartbeatInProgress = false;
  isSecure = true;
  websockets::WebsocketsClient::connect(this->host.c_str(), this->port, this->path.c_str());
}

void WebsocketClient::resetStreaming() {
    isStreaming = false;
}

bool WebsocketClient::stream() {
  return websockets::WebsocketsClient::stream();
}

bool WebsocketClient::send(uint8_t *payload, size_t length, bool headerToPayload) {
  return send((const char *) payload, length, headerToPayload);
}

bool WebsocketClient::send(const char *payload, size_t length, bool headerToPayload) {
  WS_DEBUG("Sending message of length %d\n", length);
  if (length == 0) {
    length = strlen(payload);
  }

  return websockets::WebsocketsClient::send((const char *) payload, length);
}

bool WebsocketClient::end() {
  return websockets::WebsocketsClient::end();
}

#endif