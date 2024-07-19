#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#if defined(ARDUINO)
#include <Arduino.h>
#include <WebSocketsClient.h>
typedef String WSInterfaceString;
#else
#include <tiny_websockets/client.hpp>
#include <sys/time.h>
#include <functional>
typedef const char* WSInterfaceString;
#endif

#ifdef SERIAL_DEBUG
#if defined(ARDUINO)
#define WS_DEBUG(...)          \
  Serial.print("Websocket: "); \
  Serial.printf(__VA_ARGS__)
#else
#define WS_DEBUG(...)          \
  printf("Websocket: "); \
  printf(__VA_ARGS__)
#endif
#else
#define WS_DEBUG(...)
#endif

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

#if defined(ARDUINO)
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

#else
unsigned long millis();

class WebsocketClient : protected websockets::WebsocketsClient {
public:
  WebsocketClient() : websockets::WebsocketsClient() {
    websockets::WebsocketsClient::onEvent([this](websockets::WebsocketsEvent event, websockets::WSInterfaceString message) {
      switch (event) {
        case websockets::WebsocketsEvent::GotPing:
        // Respond to the ping
          WS_DEBUG("Ponged a ping\n");
          pong(message);
          _callback(WSEvent_PING, (uint8_t *) message.c_str(), message.length());
          break;
        case websockets::WebsocketsEvent::GotPong:
          WS_DEBUG("Received a pong\n");
          if (heartbeatEnabled) {
            // If heartbeat is enabled, reset the missed coun and set the heartbeat in progress flag to false
            heartbeatMissed = 0;
            heartbeatInProgress = false;
          }
          _callback(WSEvent_PONG, (uint8_t *) message.c_str(), message.length());
          break;
        case websockets::WebsocketsEvent::ConnectionOpened:
          WS_DEBUG("Connection opened\n");
          _callback(WSEvent_CONNECTED, (uint8_t *) message.c_str(), message.length());
          break;
          case websockets::WebsocketsEvent::ConnectionClosed:
          WS_DEBUG("Connection closed\n");
          // If the connection was closed, set the heartbeat in progress flag to false
          if (heartbeatEnabled) {
            heartbeatMissed = 0;
            heartbeatInProgress = false;
          }
          _callback(WSEvent_DISCONNECTED, (uint8_t *) message.c_str(), message.length());
          break;
      }
    });

    websockets::WebsocketsClient::onMessage([this](websockets::WebsocketsMessage message) -> void {
        WS_DEBUG("Received websocket message\n")
        switch (message.type()) {
            case websockets::MessageType::Text:
                WS_DEBUG("get text: %s\n", payload);
                _callback(WSEvent_TEXT, (uint8_t *) message.c_str(), message.length());
                break;
            case websockets::MessageType::Binary:
                WS_DEBUG("get binary length: %u\n", length);
                _callback(WSEvent_BIN, (uint8_t *) message.c_str(), message.length());
                break;
            case websockets::MessageType::Ping:
            case websockets::MessageType::Pong:
                //Already handled
                break;
            default:
                WS_DEBUG("unsupported message type %u\n", message.type());
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

//   /**
//    * @brief Connect to a websocket server using a secure connection
//    * 
//    * @param host String containing the host name or IP address of the server
//    * @param port Port number to connect to
//    * @param path Path to connect to on the server
//    */
//   void connectSecure(WSInterfaceString host, int port, WSInterfaceString path);

  /**
   * @brief Close the connection to the websocket server
   * 
   */
  void close() {
    shouldReconnect = false;
    heartbeatMissed = 0;
    heartbeatInProgress = false;
    websockets::WebsocketsClient::close();
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
  unsigned int heartbeatInterval = 0;
  unsigned int heartbeatTimeout = 0;
  unsigned long heartbeatLastSent = 0;
  unsigned int heartbeatMissed = 0;
  unsigned int heartbeatMaxMissed = 0;
  bool heartbeatInProgress = false;
  bool heartbeatEnabled = false;

  unsigned int reconnectInterval = 500;
  unsigned long reconnectLastAttempt = 0;
  bool shouldReconnect = false;

  WSInterfaceString host;
  int port;
  WSInterfaceString path;

  WebSocketEventCallback eventCallback = nullptr;

  void _callback(WSEvent_t type, uint8_t * payload, size_t length) {
    if (eventCallback) {
      eventCallback(type, payload, length);
    }
  }

  bool done = false;

  bool isStreaming = false;
};

#endif

#endif