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
        // Respond to the ping
          WS_DEBUG("Ponged a ping\n");
          pong(message);
          break;
        case websockets::WebsocketsEvent::GotPong:
          WS_DEBUG("Received a pong\n");
          if (webSocketHeartbeatEnabled) {
            // If heartbeat is enabled, reset the missed coun and set the heartbeat in progress flag to false
            webSocketHeartbeatMissed = 0;
            webSocketHeartbeatInProgress = false;
          }
          break;
        case websockets::WebsocketsEvent::ConnectionOpened:
          WS_DEBUG("Connection opened\n");
          // Mark the first attempt to reconnect as false, so it will use the slower reconnect interval
          webSocketReconnectFirstAttempt = false;
          break;
          case websockets::WebsocketsEvent::ConnectionClosed:
          WS_DEBUG("Connection closed\n");
          // If the connection was closed, set the heartbeat in progress flag to false
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

  /**
   * @brief Connect to a websocket server
   * 
   * @param host String containing the host name or IP address of the server
   * @param port Port number to connect to
   * @param path Path to connect to on the server
   * @return true Connection was successful
   * @return false Connection was unsuccessful
   */
  bool connect(websockets::WSInterfaceString host, int port, websockets::WSInterfaceString path);

  /**
   * @brief Connect to a websocket server using a secure connection
   * 
   * @param host String containing the host name or IP address of the server
   * @param port Port number to connect to
   * @param path Path to connect to on the server
   * @return true Connection was successful
   * @return false Connection was unsuccessful
   */
  bool connectSecure(websockets::WSInterfaceString host, int port, websockets::WSInterfaceString path);

  /**
   * @brief Close the connection to the websocket server
   * 
   */
  void close() {
    WS_DEBUG("Closing connection\n");
    webSocketShouldReconnect = false;
    webSocketHeartbeatMissed = 0;
    webSocketHeartbeatInProgress = false;
    websockets::WebsocketsClient::close();
  }

  /**
   * @brief Enable a heartbeat to keep the connection alive
   * 
   * @param interval Time in milliseconds between heartbeats
   * @param timeout Time in milliseconds to wait for a response to the heartbeat
   * @param maxMissed Maximum number of missed heartbeats before closing the connection
   */
  void enableWebSocketHeartbeat(unsigned long interval, unsigned long timeout, unsigned long maxMissed);

  /**
   * @brief Disable the heartbeat
   * 
   */
  void disableWebSocketHeartbeat();

  /**
   * @brief Enable automatic reconnection to the websocket server
   * 
   * @param firstInterval Time in milliseconds to wait before the first successful connection attempt
   * @param interval Time in milliseconds between reconnection attempts
   */
  void enableWebSocketReconnect(unsigned long firstInterval, unsigned long interval);

  /**
   * @brief Disable automatic reconnection
   * 
   */
  void disableWebSocketReconnect();

  /**
   * @brief Poll the websocket connection
   * 
   */
  void poll();

  /**
   * @brief Set the callback function to run when an event occurs
   * 
   * @param callback Function to run when an event occurs
   */
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

  bool isSecure = false;
};

#endif