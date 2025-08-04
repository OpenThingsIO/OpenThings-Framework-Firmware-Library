#include "OpenThingsFramework.h"
#include "StringBuilder.hpp"
#include <string>

// The timeout for reading and parsing incoming requests.
#define WIFI_CONNECTION_TIMEOUT 1500
/* How often to try to reconnect to the websocket if the connection is lost. Each reconnect attempt is blocking and has
 * a 5 second timeout.
 */
#define WEBSOCKET_RECONNECT_INTERVAL 5000

using namespace OTF;

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, char *hdBuffer, int hdBufferSize) : localServer(webServerPort) {
  OTF_DEBUG("Instantiating OTF...\n");
  if(hdBuffer != NULL) { // if header buffer is externally provided, use it directly
    headerBuffer = hdBuffer;
    headerBufferSize = (hdBufferSize > 0) ? hdBufferSize : HEADERS_BUFFER_SIZE;
  } else { // otherwise allocate one
    headerBuffer = new char[HEADERS_BUFFER_SIZE];
    headerBufferSize = HEADERS_BUFFER_SIZE;
  }
  missingPageCallback = defaultMissingPageCallback;
  localServer.begin();
};

#if defined(ARDUINO)
OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, const String &webSocketHost, uint16_t webSocketPort,
                                         const String &deviceKey, bool useSsl, char *hdBuffer, int hdBufferSize) : OpenThingsFramework(webServerPort, hdBuffer, hdBufferSize) {
#else
OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, const char* webSocketHost, uint16_t webSocketPort,
                                         const char* deviceKey, bool useSsl, char *hdBuffer, int hdBufferSize) : OpenThingsFramework(webServerPort, hdBuffer, hdBufferSize) {
#endif
  setCloudStatus(UNABLE_TO_CONNECT);
  OTF_DEBUG(F("Initializing websocket...\n"));
  webSocket = new WebsocketClient();

  // Wrap the member function in a static function.
  webSocket->onEvent([this](WSEvent_t type, uint8_t *payload, size_t length) -> void {
    OTF_DEBUG((char *) F("Received websocket event of type %d\n"), type);
    webSocketEventCallback(type, payload, length);
  });

  if (useSsl) {
    OTF_DEBUG(F("Connecting to websocket with SSL\n"));
    // #if defined(ARDUINO)
    // webSocket->connectSecure(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
    // #else
    // std::string path = std::string("/socket/v1?deviceKey=") + deviceKey;
    // webSocket->connectSecure(std::string(webSocketHost), webSocketPort, path);
    // #endif
  } else {
    OTF_DEBUG(F("Connecting to websocket without SSL\n"));
    #if defined(ARDUINO)
    webSocket->connect(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
    #else
    std::string path = std::string("/socket/v1?deviceKey=") + deviceKey;
    webSocket->connect(std::string(webSocketHost), webSocketPort, path);
    #endif
  }
  OTF_DEBUG(F("Initialized websocket\n"));

  // Try to reconnect to the websocket if the connection is lost.
  webSocket->setReconnectInterval(WEBSOCKET_RECONNECT_INTERVAL);
  // Ping the server every 15 seconds with a timeout of 5 seconds, and treat 1 missed ping as a lost connection.
  webSocket->enableHeartbeat(15000, 5000, 1);
}

char *makeMapKey(StringBuilder *sb, HTTPMethod method, const char *path) {
  sb->bprintf(F("%d%s"), method, path);
  return sb->toString();
}

void OpenThingsFramework::on(const char *path, callback_t callback, HTTPMethod method) {
  callbacks.add(makeMapKey(new StringBuilder(KEY_MAX_LENGTH), method, path), callback);
}

#if defined(ARDUINO)
void OpenThingsFramework::on(const __FlashStringHelper *path, callback_t callback, HTTPMethod method) {
  callbacks.add(makeMapKey(new StringBuilder(KEY_MAX_LENGTH), method, (char *) path), callback);
}
#endif

void OpenThingsFramework::onMissingPage(callback_t callback) {
  missingPageCallback = callback;
}

void OpenThingsFramework::localServerLoop() {

  static unsigned long wait_to = 0; // timeout to wait for client data
  if (!wait_to) {
    localClient = localServer.acceptClient();
    // If a client wasn't available from the server, exit the local server loop.
    if (!localClient) {
      return;
    }
    // set a timeout to wait for client data
    wait_to = millis()+WIFI_CONNECTION_TIMEOUT;
  }
  if (!localClient->dataAvailable()) {
    // If data isn't available from the client yet, exit the local server loop and check again next iteration.
    // but if we reached timeout, then reset wait_to to 0 and flush localClient so we can accept new client
    if(millis()>wait_to) {
      wait_to=0;
      OTF_DEBUG(F("client wait timeout\n"));
      localClient->flush();
      localClient->stop();
    }
    return;
  }
  // got new client data, reset wait_to to 0
  wait_to = 0;


  // Update the timeout for each data read to ensure that the total timeout is WIFI_CONNECTION_TIMEOUT.
  #if defined(ARDUINO)
  unsigned int timeout = millis()+WIFI_CONNECTION_TIMEOUT;
  #else
  unsigned long timeout = millis()+WIFI_CONNECTION_TIMEOUT;
  #endif


  char *buffer = headerBuffer;
  size_t length = 0;
  while (localClient->dataAvailable()&&millis()<timeout) {
    if (length >= headerBufferSize) {
      localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
      // Get a new client to indicate that the previous client is no longer needed.
      localClient = localServer.acceptClient();
      return;
    }

    size_t size = 
    #if defined(ARDUINO)
    min
    #else
    std::min
    #endif
    ((int) (headerBufferSize - length - 1), headerBufferSize);
    
    size_t read = localClient->readBytesUntil('\n', &buffer[length], size);
    char rc = buffer[length];
    length += read;
    buffer[length++] = '\n';
    if(read==1 && rc=='\r') { break; }
  }
  OTF_DEBUG((char *) F("Finished reading data from client. Request line + headers were %d bytes\n"), length);
  buffer[length] = 0;

  // Make sure that the headers were fully read into the buffer.
  if (strncmp_P(&buffer[length - 4], (char *) F("\r\n\r\n"), 4) != 0) {
    OTF_DEBUG(F("The request headers were not fully read into the buffer.\n"));
    localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
    return;
  }

  OTF_DEBUG(F("Parsing request"));
  Request request(buffer, length, false);

  char *bodyBuffer = NULL;
  // If the request was valid, read the body and add it to the Request object.
  if (request.getType() > INVALID) {
    char *contentLengthString = request.getHeader(F("content-length"));
    // If the header was not specified, the message has no body.
    if (contentLengthString != nullptr) {
        #if defined(ARDUINO)
      long contentLength = String(contentLengthString).toInt();
      #else
      long contentLength = atol(contentLengthString);
      #endif
      // If the header specifies a length of 0 or could not be parsed, the message has no body.
      if (contentLength > 0) {
        // Read the body from the client.
        bodyBuffer = new char[contentLength];
        size_t bodyLength = 0;
        timeout = millis()+WIFI_CONNECTION_TIMEOUT;
        while (localClient->dataAvailable() && millis()<timeout) {
          size_t size = 
          #if defined(ARDUINO)
          min
          #else
          std::min
          #endif
          ((int) (contentLength - bodyLength), 1024);
          size_t read = localClient->readBytes(&bodyBuffer[bodyLength], size);
          bodyLength += read;
        }
        bodyBuffer[bodyLength] = 0;
        request.body = bodyBuffer;
        request.bodyLength = bodyLength;
      }
    }
  }

  // Make response stream to client
  Response res = Response();
  res.enableStream([this](const char *buffer, size_t length, bool first_message) -> void {
    localClient->write(buffer, length);
  }, [this]() -> void {
    localClient->flush();
  }, [this]() -> void {
    localClient->flush();
  });
  fillResponse(request, res);

  // Make sure to end the stream if it was enabled.
  res.end();

  if(bodyBuffer) delete[] bodyBuffer;
  if (res.isValid()) {
    OTF_DEBUG("Sent response, %d bytes\n", res.getTotalLength());
  } else {
    localClient->print(F("HTTP/1.1 500 OTF error\r\nResponse string could not be built\r\n"));
    OTF_DEBUG(F("An error occurred while building the response string.\n"));
  }

  // Properly close the client connection.
  localClient->flush();
  localClient->stop();

  // Get a new client to indicate that the previous client is no longer needed.
  localClient = localServer.acceptClient();
  if (localClient) {
    OTF_DEBUG(F("Accepted new client\n"));
    wait_to = millis()+WIFI_CONNECTION_TIMEOUT;
  }

  OTF_DEBUG(F("Finished handling request\n"));
}

void OpenThingsFramework::loop() {
  localServerLoop();
  if (webSocket != nullptr) {
    webSocket->poll();
  }
}

void OpenThingsFramework::webSocketEventCallback(WSEvent_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WSEvent_DISCONNECTED: {
      OTF_DEBUG(F("Websocket connection closed\n"));
      if (cloudStatus == CONNECTED) {
        // Make sure the cloud status is only set to disconnected if it was previously connected.
        setCloudStatus(DISCONNECTED);
        this->webSocket->resetStreaming();
      }
      break;
    }

    case WSEvent_CONNECTED: {
      OTF_DEBUG(F("Websocket connection opened\n"));
      setCloudStatus(CONNECTED);
      this->webSocket->resetStreaming();
      break;
    }

    case WSEvent_PING: {
      OTF_DEBUG(F("Received a ping from the server\n"));
      break;
    }

    case WSEvent_PONG: {
      OTF_DEBUG(F("Received a pong from the server\n"));
      break;
    }

    case WSEvent_TEXT: {
      #define PREFIX_LENGTH 5
      #define ID_LENGTH 4
      // Length of the prefix, request ID, carriage return, and line feed.
      #define HEADER_LENGTH PREFIX_LENGTH + ID_LENGTH + 2

      char *message_data = (char*) payload;

      if (strncmp_P(message_data, (char *) F("FWD: "), PREFIX_LENGTH) == 0) {
        OTF_DEBUG(F("Message is a forwarded request.\n"));
        char *requestId = &message_data[PREFIX_LENGTH];
        // Replace the assumed carriage return with a null character to terminate the ID string.
        requestId[ID_LENGTH] = '\0';

        Request request(&message_data[HEADER_LENGTH], length - HEADER_LENGTH, true);
        Response res = Response();
        // Make response stream to websocket
        res.enableStream([this] (const char *buffer, size_t length, bool first_message) -> void {
          // If the websocket is not already streaming, start streaming.
          if (first_message) {
            WS_DEBUG("Starting stream\n");
            webSocket->stream();
          }

          // Send the buffer to the websocket stream.
          webSocket->send(buffer, length);
        }, [this] () -> void {
          // Flush the websocket stream.
          webSocket->send("", 0);
        }, [this] () -> void {
          // End the websocket stream.
          webSocket->end();
        });

        res.bprintf(F("RES: %s\r\n"), requestId);
        fillResponse(request, res);
        // Make sure to end the stream if it was enabled.
        res.end();

        if (res.isValid()) {
          OTF_DEBUG("Sent response, %d bytes\n", res.getTotalLength());
        } else {
          OTF_DEBUG(F("An error occurred building response string\n"));
          StringBuilder builder(100);
          builder.bprintf(F("RES: %s\r\n%s"), requestId,
                          F("HTTP/1.1 500 Internal Error\r\n\r\nAn internal error occurred"));
          if (!builder.isValid()) {
            OTF_DEBUG(F("Builder is not valid\n"));
            return;
          }
        }
      } else {
        OTF_DEBUG(F("Websocket message does not start with the correct prefix.\n"));
      }
      break;
    }
    
    default: {
      OTF_DEBUG((char *) F("Received unsupported websocket event of type %d\n"), type);
      break;
    }
  }
}

void OpenThingsFramework::fillResponse(const Request &req, Response &res) {
  if (req.getType() == INVALID) {
    res.writeStatus(400, F("Invalid request"));
    res.writeHeader(F("content-type"), F("text/plain"));
    res.writeBodyChunk(F("Could not parse request"));
    return;
  }

  // TODO handle trailing slash in path?
  OTF_DEBUG((char *) F("Attempting to route request to path '%s'\n"), req.getPath());
  StringBuilder *sb = new StringBuilder(KEY_MAX_LENGTH);
  char *key = makeMapKey(sb, req.httpMethod, req.getPath());
  callback_t callback = callbacks.find(key);

  // If there isn't a callback for the specific method, check if there's one for any method.
  if (callback == nullptr) {
    delete sb;
    sb = new StringBuilder(KEY_MAX_LENGTH);

    callback = callbacks.find(makeMapKey(sb, HTTP_ANY, req.getPath()));
  }

  delete sb;

  if (callback != nullptr) {
    OTF_DEBUG(F("Found callback\n"));
    callback(req, res);
  } else {
    // Run the missing page callback if none of the registered paths matched.
    missingPageCallback(req, res);
  }
}

void OpenThingsFramework::defaultMissingPageCallback(const Request &req, Response &res) {
  res.writeStatus(404, F("Not found"));
  res.writeHeader(F("content-type"), F("text/plain"));
  res.writeBodyChunk(F("The requested page does not exist"));
}

void OpenThingsFramework::setCloudStatus(CLOUD_STATUS status) {
  this->cloudStatus = status;
  lastCloudStatusChangeTime = millis();
}

CLOUD_STATUS OpenThingsFramework::getCloudStatus() {
  return cloudStatus;
}

unsigned long OpenThingsFramework::getTimeSinceLastCloudStatusChange() {
  return millis() - lastCloudStatusChangeTime;
}
