#include "Websocket.h"

void WebsocketClient::enableWebSocketHeartbeat(unsigned long interval, unsigned long timeout, unsigned long maxMissed) {
  webSocketHeartbeatInterval = interval;
  webSocketHeartbeatTimeout = timeout;
  webSocketHeartbeatMaxMissed = maxMissed;
  webSocketHeartbeatEnabled = true;
}

void WebsocketClient::disableWebSocketHeartbeat() {
  webSocketHeartbeatEnabled = false;
}

void WebsocketClient::enableWebSocketReconnect(unsigned long firstInterval, unsigned long interval) {
    webSocketFirstReconnectInterval = firstInterval;
  webSocketReconnectInterval = interval;
  webSocketReconnectEnabled = true;
}

void WebsocketClient::disableWebSocketReconnect() {
  webSocketReconnectEnabled = false;
}

void WebsocketClient::poll() {
  websockets::WebsocketsClient::poll();
  if (webSocketHeartbeatEnabled && available()) {
    if (!webSocketHeartbeatInProgress && (millis() - webSocketHeartbeatLastSent > webSocketHeartbeatInterval)) {
      if (webSocketHeartbeatMissed >= webSocketHeartbeatMaxMissed) {
        // Too many missed heartbeats, close the connection
        WS_DEBUG("Too many missed heartbeats, closing connection\n");
        webSocketReconnectLastAttempt = 0;
        webSocketHeartbeatMissed = 0;
        websockets::WebsocketsClient::close();
        return;
      }

      WS_DEBUG("Sending ping\n");
      ping();
      webSocketHeartbeatLastSent = millis();
      webSocketHeartbeatInProgress = true;
    }

    if (webSocketHeartbeatInProgress && (millis() - webSocketHeartbeatLastSent > webSocketHeartbeatTimeout)) {
      // Heartbeat timeout
      WS_DEBUG("Heartbeat timeout\n");
      webSocketHeartbeatMissed++;
      webSocketHeartbeatInProgress = false;
      return;
    }
  }

  if (webSocketReconnectEnabled && webSocketShouldReconnect && !available()) {
    if (millis() - webSocketReconnectLastAttempt > (webSocketReconnectFirstAttempt ? webSocketFirstReconnectInterval : webSocketReconnectInterval)) {
      WS_DEBUG("Reconnecting...\n");
      // Attempt to reconnect
      if (isSecure) {
        websockets::WebsocketsClient::connectSecure(host, port, path);
      } else {
        websockets::WebsocketsClient::connect(host, port, path);
      }

      WS_DEBUG("Reconnect attempt complete\n");
      WS_DEBUG("Connection status: %d\n", websockets::WebsocketsClient::available());
      webSocketReconnectLastAttempt = millis();
    }
  }

  websockets::WebsocketsClient::poll();
}

void WebsocketClient::onEvent(websockets::PartialEventCallback callback) {
  WS_DEBUG("Setting event callback\n");
  this->eventCallback = [callback](WebsocketsClient &, websockets::WebsocketsEvent event, websockets::WSInterfaceString data) {
    callback(event, data);
  };
}

bool WebsocketClient::connect(websockets::WSInterfaceString host, int port, websockets::WSInterfaceString path) {
  WS_DEBUG("Connecting to ws://%s:%d%s\n", host.c_str(), port, path.c_str());
  this->host = host;
  this->port = port;
  this->path = path;
  webSocketReconnectFirstAttempt = true;
  webSocketShouldReconnect = true;
  webSocketHeartbeatMissed = 0;
  webSocketHeartbeatInProgress = false;
  isSecure = false;
  return websockets::WebsocketsClient::connect(host, port, path);
}

bool WebsocketClient::connectSecure(websockets::WSInterfaceString host, int port, websockets::WSInterfaceString path) {
  WS_DEBUG("Connecting to wss://%s:%d%s\n", host.c_str(), port, path.c_str());
  this->host = host;
  this->port = port;
  this->path = path;
  webSocketReconnectFirstAttempt = true;
  webSocketShouldReconnect = true;
  webSocketHeartbeatMissed = 0;
  webSocketHeartbeatInProgress = false;
  isSecure = true;
  return websockets::WebsocketsClient::connectSecure(host, port, path);
}