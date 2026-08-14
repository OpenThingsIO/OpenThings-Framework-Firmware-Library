#include "OpenThingsFramework.h"
#include "HttpParser.h"
#include "StringBuilder.hpp"
#include <string>
#include <stdlib.h>
#include <new>

#if !defined(ARDUINO)
#include <unistd.h>
#endif

// The timeout for reading and parsing incoming requests.
#define WIFI_CONNECTION_TIMEOUT 1500
/* How often to try to reconnect to the websocket if the connection is lost. Each reconnect attempt is blocking and has
 * a 5 second timeout.
 */
#define WEBSOCKET_RECONNECT_INTERVAL 5000
// Upper bound on Content-Length we'll allocate for the request body. Anything
// larger is rejected with 413 — protects against rogue/malformed clients
// declaring huge bodies that would exhaust heap on small targets.
#ifndef OTF_MAX_BODY_SIZE
#define OTF_MAX_BODY_SIZE 8192
#endif

// Reserve space for the normalized request terminator and intermediate writes.
static const size_t HEADER_BUFFER_RESERVED_BYTES = 10;

using namespace OTF;

static void waitForRequestData() {
#if defined(ARDUINO)
  delay(1);
#else
  usleep(1000);
#endif
}

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, char *hdBuffer, int hdBufferSize) : localServer(webServerPort) {
  OTF_DEBUG("Instantiating OTF...\n");
  if(hdBuffer != NULL) { // if header buffer is externally provided, use it directly
    headerBuffer = hdBuffer;
    headerBufferSize = (hdBufferSize > 0) ? static_cast<size_t>(hdBufferSize) : HEADERS_BUFFER_SIZE;
    ownsHeaderBuffer = false;
  } else { // otherwise allocate one
    headerBuffer = new (std::nothrow) char[HEADERS_BUFFER_SIZE];
    headerBufferSize = headerBuffer ? HEADERS_BUFFER_SIZE : 0;
    ownsHeaderBuffer = true;
  }
  if (headerBuffer && headerBufferSize > 0) headerBuffer[0] = '\0';
  missingPageCallback = defaultMissingPageCallback;
  localServer.begin();
};

OpenThingsFramework::~OpenThingsFramework() {
  delete webSocket;
  if (ownsHeaderBuffer) {
    delete[] headerBuffer;
  }
}

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
    // webSocket->connectSecure(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
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

// Allocate a route key sized exactly for "<methodDigit><path>". Caller passes
// the resulting pointer to LinkedMap::addOwned, which transfers ownership to
// the node and frees it on destruction.
static char *makeStoredKey(HTTPMethod method, const char *path) {
  int len = snprintf(nullptr, 0, "%d%s", (int)method, path);
  char *key = new char[len + 1];
  snprintf(key, len + 1, "%d%s", (int)method, path);
  return key;
}

void OpenThingsFramework::on(const char *path, callback_t callback, HTTPMethod method) {
  callbacks.addOwned(makeStoredKey(method, path), callback);
}

#if defined(ARDUINO)
void OpenThingsFramework::on(const __FlashStringHelper *path, callback_t callback, HTTPMethod method) {
  // Cast: works on ESP8266/ESP32 where flash is byte-addressable like RAM.
  // Not portable to AVR — same constraint as the previous makeMapKey path.
  callbacks.addOwned(makeStoredKey(method, (const char *)path), callback);
}
#endif

void OpenThingsFramework::onMissingPage(callback_t callback) {
  missingPageCallback = callback;
}

bool OpenThingsFramework::localServerLoop() {

  static uint32_t wait_to = 0; // timeout to wait for client data
  auto closeCurrentClient = [this]() {
    localClient->flush();
    localClient->stop();
    localClient = localServer.acceptClient();
    wait_to = localClient ? millis() + WIFI_CONNECTION_TIMEOUT : 0;
  };
  if (!wait_to) {
    localClient = localServer.acceptClient();
    // If a client wasn't available from the server, exit the local server loop.
    if (!localClient) {
      return false;
    }
    // set a timeout to wait for client data
    wait_to = millis()+WIFI_CONNECTION_TIMEOUT;
  }
  if (!localClient->dataAvailable()) {
    // If data isn't available from the client yet, exit the local server loop and check again next iteration.
    // but if we reached timeout, then reset wait_to to 0 and flush localClient so we can accept new client
    if((int32_t)(millis()-wait_to) >= 0) {
      wait_to=0;
      OTF_DEBUG(F("client wait timeout\n"));
      localClient->flush();
      localClient->stop();
    }
    return true;
  }
  // got new client data, reset wait_to to 0
  wait_to = 0;

  if (!headerBuffer || headerBufferSize <= HEADER_BUFFER_RESERVED_BYTES) {
    OTF_DEBUG(F("Header buffer is unavailable or too small\n"));
    localClient->print(F("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nInsufficient header buffer"));
    closeCurrentClient();
    return true;
  }


  // Update the timeout for each data read to ensure that the total timeout is WIFI_CONNECTION_TIMEOUT.
  uint32_t timeoutStart = millis();


  char *buffer = headerBuffer;
  size_t length = 0;
  bool isFirstLine = true;

  while (localClient->dataAvailable() && millis()-timeoutStart < WIFI_CONNECTION_TIMEOUT) {
    if (isFirstLine) {
      // Read the first line (Request Line) directly into the main buffer.
      // This supports very long query strings (up to headerBufferSize).
      size_t read = localClient->readBytesUntil('\n', &buffer[length],
                                                headerBufferSize - HEADER_BUFFER_RESERVED_BYTES);
      length += read;
      buffer[length++] = '\n';
      isFirstLine = false;
      continue;
    }

    char line[128];
    size_t lineLen = localClient->readBytesUntil('\n', line, sizeof(line) - 1);
    if (lineLen == 0) break; // timeout or end of data

    // Check for empty line (end of headers)
    if (lineLen == 1 && line[0] == '\r') break;

    // Selective headers to keep
    if (strncasecmp(line, "content-length:", 15) == 0) {
      if (length + lineLen + 1 < headerBufferSize - 4) {
        memcpy(&buffer[length], line, lineLen);
        length += lineLen;
        buffer[length++] = '\n';
      }
    }
  }

  // Standardize end of headers with \r\n\r\n for the Request parser
  // (Ensuring we have at least one \r\n before the final one)
  if (length > 0 && buffer[length-1] == '\n') {
    if (length > 1 && buffer[length-2] != '\r') {
      buffer[length-1] = '\r';
      buffer[length++] = '\n';
    }
  }
  buffer[length++] = '\r';
  buffer[length++] = '\n';
  buffer[length] = 0;

  OTF_DEBUG((char *) F("Finished reading selective data from client. Stored headers size: %zu bytes\n"), length);

  OTF_DEBUG(F("Parsing request"));
  Request request(buffer, length, false);

  char *bodyBuffer = NULL;
  // If the request was valid, read the body and add it to the Request object.
  if (request.getType() > INVALID) {
    char *contentLengthString = request.getHeader(F("content-length"));
    // If the header was not specified, the message has no body.
    if (contentLengthString != nullptr) {
      size_t contentLength = 0;
      ContentLengthResult lengthResult =
        parseContentLength(contentLengthString, OTF_MAX_BODY_SIZE, contentLength);
      if (lengthResult == CONTENT_LENGTH_INVALID) {
        OTF_DEBUG(F("Invalid Content-Length\n"));
        localClient->print(F("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nInvalid Content-Length"));
        closeCurrentClient();
        return true;
      }
      if (lengthResult == CONTENT_LENGTH_TOO_LARGE) {
        OTF_DEBUG(F("Content-Length exceeds OTF_MAX_BODY_SIZE\n"));
        localClient->print(F("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\nThe request body exceeds the configured limit"));
        closeCurrentClient();
        return true;
      }
      if (contentLength > 0) {
        // Read the body from the client.
        bodyBuffer = new (std::nothrow) char[contentLength + 1];
        if (bodyBuffer == nullptr) {
          OTF_DEBUG(F("Could not allocate request body buffer\n"));
          localClient->print(F("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nInsufficient memory"));
          closeCurrentClient();
          return true;
        }
        size_t bodyLength = 0;
        timeoutStart = millis();
        while (bodyLength < contentLength &&
               millis() - timeoutStart < WIFI_CONNECTION_TIMEOUT) {
          size_t available = localClient->availableBytes();
          if (available == 0) {
            waitForRequestData();
            continue;
          }
          size_t size =
          #if defined(ARDUINO)
          min
          #else
          std::min
          #endif
          (contentLength - bodyLength, (size_t)1024);
          size =
          #if defined(ARDUINO)
          min
          #else
          std::min
          #endif
          (size, available);
          size_t read = localClient->readBytes(&bodyBuffer[bodyLength], size);
          if (read == 0) {
            waitForRequestData();
            continue;
          }
          bodyLength += read;
        }
        if (bodyLength != contentLength) {
          delete[] bodyBuffer;
          bodyBuffer = nullptr;
          OTF_DEBUG(F("Timed out before receiving the complete request body\n"));
          localClient->print(F("HTTP/1.1 408 Request Timeout\r\nConnection: close\r\n\r\nIncomplete request body"));
          closeCurrentClient();
          return true;
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
  return true;
}

void OpenThingsFramework::loop(bool allowCloud) {
  bool localClientActive = localServerLoop();
  if (!localClientActive && allowCloud && webSocket != nullptr) {
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
      }
      // Clear any in-progress stream state — a disconnect during a response
      // would otherwise leave isStreaming stuck true and break subsequent
      // streams after reconnect.
      this->webSocket->resetStreaming();
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
  // Stack buffer for lookup keys — registered route keys are short
  // (OS firmware's longest is ~5 chars including method digit + null).
  // If the formatted key would not fit, the lookup is skipped — a request
  // path longer than this cap falls through to missingPageCallback rather
  // than risk a misroute via a truncated key.
  char lookupKey[64];
  callback_t callback = nullptr;
  int n = snprintf(lookupKey, sizeof(lookupKey), "%d%s", (int)req.httpMethod, req.getPath());
  if (n > 0 && (size_t)n < sizeof(lookupKey)) {
    callback = callbacks.find(lookupKey);
  }
  // If there isn't a callback for the specific method, check if there's one for any method.
  if (callback == nullptr) {
    n = snprintf(lookupKey, sizeof(lookupKey), "%d%s", (int)HTTP_ANY, req.getPath());
    if (n > 0 && (size_t)n < sizeof(lookupKey)) {
      callback = callbacks.find(lookupKey);
    }
  }

  OTF_DEBUG((char *) F("callback=%x\n"), callback);
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

uint32_t OpenThingsFramework::getTimeSinceLastCloudStatusChange() {
  return millis() - lastCloudStatusChangeTime;
}
