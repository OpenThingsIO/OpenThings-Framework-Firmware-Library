#include "OpenThingsFramework.h"
#include "StringBuilder.hpp"
#include <string>

#if defined(ARDUINO) && defined(ESP32)
#include <esp_heap_caps.h>
#endif

// The timeout for reading and parsing incoming requests.
#define WIFI_CONNECTION_TIMEOUT 1500
/* How often to try to reconnect to the websocket if the connection is lost. Each reconnect attempt is blocking and has
 * a 5 second timeout.
 */
#define WEBSOCKET_RECONNECT_INTERVAL 5000

using namespace OTF;

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, char *hdBuffer, int hdBufferSize)
#if defined(ARDUINO) && defined(ESP32)
    : localServer(webServerPort, webServerPort + 363)
#else
    : localServer(webServerPort)
#endif
{
  OTF_DEBUG("Instantiating OTF...\n");
#if defined(ARDUINO) && defined(ESP32)
  OTF_DEBUG("HTTP port: %d, HTTPS port: %d\n", webServerPort, webServerPort + 363);
#else
  OTF_DEBUG("HTTP port: %d\n", webServerPort);
#endif
  if(hdBuffer != NULL) { // if header buffer is externally provided, use it directly
    headerBuffer = hdBuffer;
    headerBufferSize = (hdBufferSize > 0) ? hdBufferSize : HEADERS_BUFFER_SIZE;
  } else { // otherwise allocate from PSRAM if available, then heap
#if defined(ARDUINO) && defined(ESP32)
    headerBuffer = (char*)heap_caps_malloc(HEADERS_BUFFER_SIZE,
                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!headerBuffer) {
      headerBuffer = new char[HEADERS_BUFFER_SIZE];  // fallback to DRAM
    }
#else
    headerBuffer = new char[HEADERS_BUFFER_SIZE];
#endif
    headerBufferSize = HEADERS_BUFFER_SIZE;
  }
  missingPageCallback = defaultMissingPageCallback;
  
  OTF_DEBUG("Calling localServer.begin()...\n");
  localServer.begin();
  OTF_DEBUG("OTF instantiated and server started\n");
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
    #if defined(ARDUINO)
    webSocket->connectSecure(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
    #else
    std::string path = std::string("/socket/v1?deviceKey=") + deviceKey;
    webSocket->connectSecure(std::string(webSocketHost), webSocketPort, path);
    #endif
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
  // Ping the server every 30 seconds with a timeout of 2 seconds, and treat 3 missed ping as a lost connection.
  webSocket->enableHeartbeat(15000, 5000, 1);
}

static void makeMapKeyBuf(char *dest, size_t destSize, OTFHTTPMethod method, const char *path) {
  snprintf(dest, destSize, "%d%s", (int)method, path);
}

void OpenThingsFramework::on(const char *path, callback_t callback, OTFHTTPMethod method) {
  size_t len = strlen(path) + 4; // method digits + path + \0
  char *key = new char[len];
  makeMapKeyBuf(key, len, method, path);
  callbacks.add(key, callback);
}

#if defined(ARDUINO)
void OpenThingsFramework::on(const __FlashStringHelper *path, callback_t callback, OTFHTTPMethod method) {
  size_t pathLen = strlen_P((const char *)path);
  size_t len = pathLen + 4;
  char *key = new char[len];
  int offset = snprintf(key, 4, "%d", (int)method);
  strncpy_P(key + offset, (const char *)path, pathLen + 1);
  callbacks.add(key, callback);
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
      localClient->flush();
      localClient->stop();
      localClient = nullptr;
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
  while (millis() < timeout) {
    if (length >= (size_t)headerBufferSize - 1) {
      localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
      localClient->flush();
      localClient->stop();
      localClient = nullptr;
      return;
    }

    // `dataAvailable()` can temporarily return false between TCP packets.
    // Keep waiting (up to WIFI_CONNECTION_TIMEOUT) until the full header terminator is received.
    if (!localClient->dataAvailable()) {
      #if defined(ARDUINO)
      delay(1);
      #endif
      continue;
    }

    size_t size =
    #if defined(ARDUINO)
    min
    #else
    std::min
    #endif
    ((int)(headerBufferSize - length - 1), 256);

    size_t read = localClient->readBytesUntil('\n', &buffer[length], size);
    if (read == 0) {
      continue;
    }

    bool lineEnded = (read < size) || buffer[length + read - 1] == '\r';
    char rc = buffer[length];
    length += read;

    if (lineEnded) {
      buffer[length++] = '\n';

      if (read == 1 && rc == '\r') {
        break;
      }
      if (length >= 4 && strncmp_P(&buffer[length - 4], (char *)F("\r\n\r\n"), 4) == 0) {
        break;
      }
    }
  }
  OTF_DEBUG((char *) F("Finished reading data from client. Request line + headers were %d bytes\n"), length);
  buffer[length] = 0;

  // Make sure that the headers were fully read into the buffer.
  if (length < 4 || strncmp_P(&buffer[length - 4], (char *) F("\r\n\r\n"), 4) != 0) {
    OTF_DEBUG(F("The request headers were not fully read into the buffer.\n"));
    localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
    return;
  }

  OTF_DEBUG(F("Parsing request\n"));
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
        // Read the body from the client (+1 for NUL terminator).
        bodyBuffer = new char[contentLength + 1];
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
  OTF_DEBUG(F("Setting up response stream for local client\n"));
  res.enableStream([this](const char *buffer, size_t length, bool first_message) -> void {
    OTF_DEBUG(F("Stream write: %d bytes, first=%d\n"), length, first_message);
    localClient->write(buffer, length);
  }, [this]() -> void {
    localClient->flush();
  }, [this]() -> void {
    localClient->flush();
  });
  fillResponse(request, res);

  OTF_DEBUG(F("Before res.end(): valid=%d, length=%d\n"), res.isValid(), res.getTotalLength());
  // Make sure to end the stream if it was enabled.
  res.end();
  OTF_DEBUG(F("After res.end(): valid=%d, length=%d\n"), res.isValid(), res.getTotalLength());

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
  localClient = nullptr;

  // localClient is now null — next loop iteration will accept a new one
  OTF_DEBUG(F("Finished handling request\n"));
}

void OpenThingsFramework::loop() {
  localServerLoop();
  if (webSocket != nullptr) {
    webSocket->poll();
  }
}

void OpenThingsFramework::pollCloud() {
  if (webSocket != nullptr) {
    webSocket->poll();
  }
}

void OpenThingsFramework::disconnectCloud() {
  if (webSocket != nullptr) {
    webSocket->close();
    webSocket->setReconnectInterval(3600000); // Suppress auto-reconnect (1 hour)
  }
}

void OpenThingsFramework::reconnectCloud() {
  if (webSocket != nullptr) {
    webSocket->setReconnectInterval(WEBSOCKET_RECONNECT_INTERVAL); // Restore normal reconnect
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
          //webSocket->flush();
        }, [this] () -> void {
          // End the websocket stream.
          webSocket->end();
        });

        res.appendStr(F("RES: "));
        res.appendStr(requestId);
        res.appendStr(F("\r\n"));
        //res.bprintf(F("RES: %s\r\n"), requestId);
        fillResponse(request, res);
        // Make sure to end the stream if it was enabled.
        res.end();

        if (res.isValid()) {
          OTF_DEBUG(F("Sent response, %d bytes\n"), res.getTotalLength());
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
    const char* msg = "Could not parse request";
    res.writeBodyData(msg, strlen(msg));
    return;
  }

  // TODO handle trailing slash in path?
  OTF_DEBUG((char *) F("Attempting to route request to path '%s'\n"), req.getPath());
  char lookupKey[KEY_MAX_LENGTH];
  makeMapKeyBuf(lookupKey, KEY_MAX_LENGTH, req.httpMethod, req.getPath());
  callback_t callback = callbacks.find(lookupKey);

  // If there isn't a callback for the specific method, check if there's one for any method.
  if (callback == nullptr) {
    makeMapKeyBuf(lookupKey, KEY_MAX_LENGTH, OTF_HTTP_ANY, req.getPath());
    callback = callbacks.find(lookupKey);
  }

  if (callback != nullptr) {
    OTF_DEBUG(F("Found callback\n"));
    callback(req, res);
    OTF_DEBUG(F("Callback executed, response valid: %d, total length: %d\n"), res.isValid(), res.getTotalLength());
  } else {
    OTF_DEBUG(F("No callback found, running missing page callback\n"));
    // Run the missing page callback if none of the registered paths matched.
    missingPageCallback(req, res);
    OTF_DEBUG(F("Missing page callback executed\n"));
  }
}

void OpenThingsFramework::defaultMissingPageCallback(const Request &req, Response &res) {
  res.writeStatus(404, F("Not found"));
  res.writeHeader(F("content-type"), F("text/plain"));
  const char* msg = "The requested page does not exist";
  res.writeBodyData(msg, strlen(msg));
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
