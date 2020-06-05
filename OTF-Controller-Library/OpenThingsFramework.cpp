#include "OpenThingsFramework.h"
#include "StringBuilder.h"

// The size of the buffer to store the incoming request line and headers (does not include body). Larger requests will be discarded.
#define HEADERS_BUFFER_SIZE 1024
// TODO This doesn't work because the macro is redefined in WebSockets.h
// The size of the buffer to store incoming websocket requests in. Larger requests will be discarded.
#define WEBSOCKETS_MAX_DATA_SIZE REQUEST_BUFFER_SIZE
// The timeout for reading and parsing incoming requests.
#define WIFI_CONNECTION_TIMEOUT 1500
/* How often to try to reconnect to the websocket if the connection is lost. Each reconnect attempt is blocking and has
 * a 5 second timeout.
 */
#define WEBSOCKET_RECONNECT_INTERVAL 30000

using namespace OTF;

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort) : localServer(webServerPort) {
  Serial.println("Instantiating OTF...");
  missingPageCallback = defaultMissingPageCallback;
  localServer.begin();
};

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, const String &webSocketHost, uint16_t webSocketPort,
                                         const String &deviceKey, bool useSsl) : OpenThingsFramework(webServerPort) {
  setCloudStatus(UNABLE_TO_CONNECT);
  Serial.println(F("Initializing websocket..."));
  webSocket = new WebSocketsClient();
  if (useSsl) {
    Serial.println(F("Connecting to websocket with SSL"));
    webSocket->beginSSL(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
  } else {
    Serial.println(F("Connecting to websocket without SSL"));
    webSocket->begin(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
  }
  Serial.println(F("Initialized websocket"));

  // Wrap the member function in a static function.
  webSocket->onEvent([this](WStype_t type, uint8_t *payload, size_t length) -> void {
    webSocketCallback(type, payload, length);
  });

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

void OpenThingsFramework::on(const __FlashStringHelper *path, callback_t callback, HTTPMethod method) {
  callbacks.add(makeMapKey(new StringBuilder(KEY_MAX_LENGTH), method, (char *) path), callback);
}

void OpenThingsFramework::onMissingPage(callback_t callback) {
  missingPageCallback = callback;
}

void OpenThingsFramework::localServerLoop() {
  if (!localClient) {
    // If there isn't currently a pending client, check if one is available.
    localClient = localServer.acceptClient();

    // If a client wasn't available from the server, exit the local server loop.
    if (!localClient) {
      return;
    }

    localClient->setTimeout(WIFI_CONNECTION_TIMEOUT);
  }

  if (!localClient->dataAvailable()) {
    // If data isn't available from the client yet, exit the local server loop and check again next iteration.
    return;
  }

  // Update the timeout for each data read to ensure that the total timeout is WIFI_CONNECTION_TIMEOUT.
  unsigned int timeout = WIFI_CONNECTION_TIMEOUT;
  unsigned long lastTime = millis();
#define UPDATE_TIMEOUT                        \
  {                                           \
    unsigned long diff = millis() - lastTime; \
    if (diff >= timeout) {                    \
      timeout = 0;                            \
      continue;                               \
    }                                         \
    timeout -= diff;                          \
    localClient->setTimeout(timeout);         \
  }

  char buffer[HEADERS_BUFFER_SIZE];
  size_t length = 0;
  while (localClient->dataAvailable()) {
    if (length >= HEADERS_BUFFER_SIZE) {
      Serial.println(F("Request buffer is full, aborting"));
      localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));

      // Get a new client to indicate that the previous client is no longer needed.
      localClient = localServer.acceptClient();
      return;
    }

    if (timeout <= 0) {
      Serial.println(F("Request timed out"));
      // Get a new client to indicate that the previous client is no longer needed.
      localClient = localServer.acceptClient();
      return;
    }

    size_t read = localClient->readBytesUntil('\n', &buffer[length], min((int) (HEADERS_BUFFER_SIZE - length - 1), 1024));
    length += read;
    buffer[length++] = '\n';
    UPDATE_TIMEOUT

    if (localClient->peek() == '\r') {
      UPDATE_TIMEOUT
      // Try to read the second sequential CRLF that marks the beginning of the request body.
      read = localClient->readBytesUntil('\n', &buffer[length], min((int) (HEADERS_BUFFER_SIZE - length - 1), 2));
      length += read;
      buffer[length++] = '\n';
      UPDATE_TIMEOUT
      // If exactly 2 characters were read, assume the first one was '\n' and that the end of the headers has been reached.
      if (read == 2) {
        break;
      }
    }
  }
  Serial.printf((char *) F("Finished reading data from client. Request line + headers were %d bytes\n"), length);

  // Make sure that the headers were fully read into the buffer.
  if (strncmp_P(&buffer[length - 4], (char *) F("\r\n\r\n"), 4) != 0) {
    Serial.println(F("The request headers were not fully read into the buffer."));
    localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
    return;
  }

  Serial.println(F("Parsing request"));
  Request request(buffer, length, false);

  // If the request was valid, read the body and add it to the Request object.
  if (request.getType() > INVALID) {
    char *contentLengthString = request.getHeader(F("content-length"));
    // If the header was not specified, the message has no body.
    if (contentLengthString != nullptr) {
      long contentLength = String(contentLengthString).toInt();
      // If the header specifies a length of 0 or could not be parsed, the message has no body.
      if (contentLength > 0) {
        // Read the body from the client.
        char bodyBuffer[contentLength];
        size_t bodyLength = 0;
        while (localClient->dataAvailable()) {
          UPDATE_TIMEOUT
          size_t read = localClient->readBytes(&bodyBuffer[bodyLength], min((int) (contentLength - bodyLength), 1024));
          bodyLength += read;
        }

        request.body = &bodyBuffer[0];
        request.bodyLength = bodyLength;
      }
    }
  }

  Serial.println(F("Filling response"));
  Response res = Response();
  fillResponse(request, res);

  Serial.println(F("Sending response"));
  if (res.isValid()) {
    char *responseString = res.toString();
    Serial.printf((char *) F("Response message is: %s\n"), responseString);
    localClient->print(responseString);
  } else {
    localClient->print(F("HTTP/1.1 500 OTF error\r\nResponse string could not be built\r\n"));
    Serial.println(F("An error occurred while building the response string."));
  }

  // Get a new client to indicate that the previous client is no longer needed.
  localClient = localServer.acceptClient();

  Serial.println(F("Finished handling request"));
}

void OpenThingsFramework::loop() {
  localServerLoop();
  if (webSocket != nullptr) {
    webSocket->loop();
  }
}

void OpenThingsFramework::webSocketCallback(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println(F("Disconnected from websocket"));
      /* A failed attempt to connect will also fire a WStype_DISCONNECTED event, so this check is
       * needed to prevent the UNABLE_TO_CONNECT status to be overwritten. */
      if (cloudStatus == CONNECTED) {
        setCloudStatus(DISCONNECTED);
      }
      break;
    case WStype_CONNECTED:
      Serial.println(F("Connected to websocket"));
      setCloudStatus(CONNECTED);
      break;
    case WStype_TEXT: {
      Serial.printf((char *) F("Received a websocket message of length %d\n"), length);
      char *message = (char *) payload;

#define PREFIX_LENGTH 5
#define ID_LENGTH 4
// Length of the prefix, request ID, carriage return, and line feed.
#define HEADER_LENGTH PREFIX_LENGTH + ID_LENGTH + 2
      if (strncmp_P(message, (char *) F("FWD: "), PREFIX_LENGTH) == 0) {
        Serial.println(F("Message is a forwarded request."));
        char *requestId = &message[PREFIX_LENGTH];
        // Replace the assumed carriage return with a null character to terminate the ID string.
        requestId[ID_LENGTH] = '\0';

        Request request(&message[HEADER_LENGTH], length - HEADER_LENGTH, true);
        Response res = Response();
        res.bprintf(F("RES: %s\r\n"), requestId);
        fillResponse(request, res);

        if (res.isValid()) {
          webSocket->sendTXT(res.toString(), res.getLength());
        } else {
          Serial.println(F("An error occurred building response string"));
          StringBuilder builder(100);
          builder.bprintf(F("RES: %s\r\n%s"), requestId,
                          F("HTTP/1.1 500 Internal Error\r\n\r\nAn internal error occurred"));
          if (!builder.isValid()) {
            Serial.println(F("Builder is not valid"));
            return;
          }
        }
      } else {
        Serial.println(F("Websocket message does not start with the correct prefix."));
      }
      break;
    }
    case WStype_PING:
    case WStype_PONG:
      // Pings/pongs will be automatically handled by the websockets library.
      break;
    case WStype_ERROR:
      Serial.print(F("Websocket error: "));
      Serial.println(std::string((char *) payload, length).c_str());
      break;
    default:
      Serial.printf((char *) F("Received unsupported websocket event of type %d\n"), type);
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
  Serial.printf((char *) F("Attempting to route request to path '%s'\n"), req.getPath());
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
    Serial.println(F("Found callback"));
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
