#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <ArduinoWebsockets.h>

class WebsocketClient : public websockets::WebsocketsClient {
public:
  WebsocketClient() : websockets::WebsocketsClient() {}
  void enableWebSocketHeartbeat(unsigned long interval, unsigned long timeout, unsigned long maxMissed);
  void disableWebSocketHeartbeat();

  void enableWebSocketReconnect(unsigned long interval);
  void disableWebSocketReconnect();

  void poll();

private:
  unsigned int webSocketHeartbeatInterval = 0;
  unsigned int webSocketHeartbeatTimeout = 0;
  unsigned long webSocketHeartbeatLastSent = 0;
  unsigned int webSocketHeartbeatMissed = 0;
  unsigned int webSocketHeartbeatMaxMissed = 0;
  bool webSocketHeartbeatEnabled = false;

  unsigned int webSocketReconnectInterval = 0;
  unsigned long webSocketReconnectLastAttempt = 0;
  bool webSocketReconnectEnabled = false;
  sendPing();
};

#endif