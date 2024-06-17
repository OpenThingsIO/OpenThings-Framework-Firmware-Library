#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <ArduinoWebsockets.h>

#ifdef DEBUG 
#define WS_DEBUG(...) Serial.print("Websocket: "); Serial.printf(__VA_ARGS__)
#else
#define WS_DEBUG(...)
#endif

class WebsocketClient : public websockets::WebsocketsClient {
public:
  WebsocketClient() : websockets::WebsocketsClient() {
    // Set up a callback to handle incoming pings
    websockets::WebsocketsClient::onEvent([this](websockets::WebsocketsEvent event, websockets::WSInterfaceString message) {
      switch (event) {
        case websockets::WebsocketsEvent::GotPing:
          WS_DEBUG("Ponged a ping\n");
          pong(message);
          break;
        case websockets::WebsocketsEvent::GotPong:
          WS_DEBUG("Received a pong\n");
          if (webSocketHeartbeatEnabled) {
            webSocketHeartbeatMissed = 0;
            webSocketHeartbeatInProgress = false;
          }
          break;
        case websockets::WebsocketsEvent::ConnectionOpened:
          WS_DEBUG("Connection opened\n");
          webSocketReconnectFirstAttempt = false;
          break;
          case websockets::WebsocketsEvent::ConnectionClosed:
          WS_DEBUG("Connection closed\n");
          if (webSocketHeartbeatEnabled) {
            webSocketHeartbeatMissed = 0;
            webSocketHeartbeatInProgress = false;
          }
          break;
      }

      if (eventCallback) {
        eventCallback(*this, event, message);
      }
    });
  }

  bool connect(websockets::WSInterfaceString host, int port, websockets::WSInterfaceString path);

  void close() {
    WS_DEBUG("Closing connection\n");
    webSocketShouldReconnect = false;
    webSocketHeartbeatMissed = 0;
    webSocketHeartbeatInProgress = false;
    websockets::WebsocketsClient::close();
  }

  void enableWebSocketHeartbeat(unsigned long interval, unsigned long timeout, unsigned long maxMissed);
  void disableWebSocketHeartbeat();

  void enableWebSocketReconnect(unsigned long firstInterval, unsigned long interval);
  void disableWebSocketReconnect();

  void poll();

  void onEvent(websockets::PartialEventCallback callback);

private:
  unsigned int webSocketHeartbeatInterval = 0;
  unsigned int webSocketHeartbeatTimeout = 0;
  unsigned long webSocketHeartbeatLastSent = 0;
  unsigned int webSocketHeartbeatMissed = 0;
  unsigned int webSocketHeartbeatMaxMissed = 0;
  bool webSocketHeartbeatInProgress = false;
  bool webSocketHeartbeatEnabled = false;

  unsigned int webSocketReconnectInterval = 0;
  unsigned int webSocketFirstReconnectInterval = 0;
  unsigned long webSocketReconnectLastAttempt = 0;
  bool webSocketReconnectFirstAttempt = true;
  bool webSocketReconnectEnabled = false;
  bool webSocketShouldReconnect = false;
  websockets::WSInterfaceString host;
  int port; 
  websockets::WSInterfaceString path;

  websockets::EventCallback eventCallback = nullptr;
};

#endif