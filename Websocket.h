#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <WebSocketsClient.h>

// #define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define WS_DEBUG(...)          \
  Serial.print("Websocket: "); \
  Serial.printf(__VA_ARGS__)
#else
#define WS_DEBUG(...)
#endif

typedef String WSInterfaceString;

typedef enum {
    WSEvent_ERROR,
    WSEvent_DISCONNECTED,
    WSEvent_CONNECTED,
    WSEvent_TEXT,
    WSEvent_BIN,
    // WStype_FRAGMENT_TEXT_START,
    // WStype_FRAGMENT_BIN_START,
    // WStype_FRAGMENT,
    // WStype_FRAGMENT_FIN,
    WSEvent_PING,
    WSEvent_PONG,
} WSEvent_t;

typedef std::function<void(WSEvent_t type, uint8_t * payload, size_t length)> WebSocketEventCallback;

class WebsocketClient : protected WebSocketsClient {
public:
  WebsocketClient() : WebSocketsClient() {
    // Set up a callback to handle incoming pings
    WebSocketsClient::onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
      switch (type) {
        case WStype_ERROR:
          WS_DEBUG("Error!\n");
          _callback(WSEvent_ERROR, payload, length);
          break;
        case WStype_DISCONNECTED:
          WS_DEBUG("Disconnected!\n");
          _callback(WSEvent_DISCONNECTED, payload, length);
          break;
        case WStype_CONNECTED: {
          WS_DEBUG("Connected to url: %s\n", payload);
          _callback(WSEvent_CONNECTED, payload, length);
        } break;
        case WStype_TEXT:
          WS_DEBUG("get text: %s\n", payload);
          _callback(WSEvent_TEXT, payload, length);
          break;
        case WStype_BIN:
          WS_DEBUG("get binary length: %u\n", length);
          _callback(WSEvent_BIN, payload, length);
          break;
        case WStype_PING:
          WS_DEBUG("get ping\n");
          _callback(WSEvent_PING, payload, length);
          break;
        case WStype_PONG:
          WS_DEBUG("get pong\n");
          _callback(WSEvent_PONG, payload, length);
          break;
        default:
          WS_DEBUG("Unknown event type: %d\n", type);
          break;
      }
    });
  }

  /**
   * @brief Connect to a websocket server
   * 
   * @param host String containing the host name or IP address of the server
   * @param port Port number to connect to
   * @param path Path to connect to on the server
   */
  void connect(WSInterfaceString host, int port, WSInterfaceString path);

  /**
   * @brief Connect to a websocket server using a secure connection
   * 
   * @param host String containing the host name or IP address of the server
   * @param port Port number to connect to
   * @param path Path to connect to on the server
   */
  void connectSecure(WSInterfaceString host, int port, WSInterfaceString path);

  /**
   * @brief Close the connection to the websocket server
   * 
   */
  void close() {
    WS_DEBUG("Closing connection\n");
    WebSocketsClient::disconnect();
  }

  /**
   * @brief Enable a heartbeat to keep the connection alive
   * 
   * @param interval Time in milliseconds between heartbeats
   * @param timeout Time in milliseconds to wait for a response to the heartbeat
   * @param maxMissed Maximum number of missed heartbeats before closing the connection
   */
  void enableHeartbeat(unsigned long interval, unsigned long timeout, uint8_t maxMissed);

  /**
   * @brief Disable the heartbeat
   * 
   */
  void disableHeartbeat();

  /**
   * @brief Sets the interval between reconnection attempts
   * 
   * @param interval Time in milliseconds between reconnection attempts
   */
  void setReconnectInterval(unsigned long interval);

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
  void onEvent(WebSocketEventCallback callback);

  /**
   * @brief Enable streaming mode
   * @return true Streaming mode enabled
   * @return false Streaming mode not enabled
   */
  bool stream();

  /**
   * @brief Send a text message to the server
   * @param payload Data to send
   * @param length Length of the data to send
   * @param headerToPayload bool  (see sendFrame for more details)
   * @return true Message was successful
   * @return false Message was unsuccessful
  */
  bool send(uint8_t *payload, size_t length, bool headerToPayload = false);

  /**
   * @brief Send a text message to the server
   * @param payload Data to send
   * @param length Length of the data to send
   * @param headerToPayload bool  (see sendFrame for more details)
   * @return true Message was successful
   * @return false Message was unsuccessful
  */
  bool send(const char *payload, size_t length, bool headerToPayload = false);

  /**
   * @brief End the stream
   * @return true Stream ended
   * @return false Stream failed to end
  */
  bool end();

private:
  // unsigned int webSocketHeartbeatInterval = 0;
  // unsigned int webSocketHeartbeatTimeout = 0;
  // unsigned long webSocketHeartbeatLastSent = 0;
  // unsigned int webSocketHeartbeatMissed = 0;
  // unsigned int webSocketHeartbeatMaxMissed = 0;
  // bool webSocketHeartbeatInProgress = false;
  // bool webSocketHeartbeatEnabled = false;

  // unsigned int webSocketReconnectInterval = 0;
  // unsigned int webSocketFirstReconnectInterval = 0;
  // unsigned long webSocketReconnectLastAttempt = 0;
  // bool webSocketReconnectFirstAttempt = true;
  // bool webSocketReconnectEnabled = false;
  // bool webSocketShouldReconnect = false;

  bool enableReconnect = false;
  unsigned long reconnectInterval = 0;

  WSInterfaceString host;
  int port;
  WSInterfaceString path;

  WebSocketEventCallback eventCallback = nullptr;

  void _callback(WSEvent_t type, uint8_t * payload, size_t length) {
    if (eventCallback) {
      eventCallback(type, payload, length);
    }
  }

  bool isSecure = false;

  bool isStreaming = false;
};

#endif