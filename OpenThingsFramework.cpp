#include "OpenThingsFramework.h"
#include "StringBuilder.hpp"

// The timeout for reading and parsing incoming requests.
#define WIFI_CONNECTION_TIMEOUT 1500
/* How often to try to reconnect to the websocket if the connection is lost. Each reconnect attempt is blocking and has
 * a 5 second timeout.
 */
#define WEBSOCKET_RECONNECT_INTERVAL 5000

using namespace OTF;

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, char *hdBuffer, int hdBufferSize) : localServer(webServerPort) {
  DEBUG(Serial.println("Instantiating OTF...");)
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

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, const String &webSocketHost, uint16_t webSocketPort,
                                         const String &deviceKey, bool useSsl, char *hdBuffer, int hdBufferSize) : OpenThingsFramework(webServerPort, hdBuffer, hdBufferSize) {
  setCloudStatus(UNABLE_TO_CONNECT);
  DEBUG(Serial.println(F("Initializing websocket..."));)
  webSocket = new WebsocketClient();

  // Wrap the member function in a static function.
  webSocket->onEvent([this](WSEvent_t type, uint8_t *payload, size_t length) -> void {
    DEBUG(Serial.printf((char *) F("Received websocket event of type %d\n"), event);)
    webSocketEventCallback(type, payload, length);
  });

  if (useSsl) {
    DEBUG(Serial.println(F("Connecting to websocket with SSL"));)
    // webSocket->connectSecure(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
  } else {
    DEBUG(Serial.println(F("Connecting to websocket without SSL"));)
    webSocket->connect(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
  }
  DEBUG(Serial.println(F("Initialized websocket"));)

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

void OpenThingsFramework::on(const __FlashStringHelper *path, callback_t callback, HTTPMethod method) {
  callbacks.add(makeMapKey(new StringBuilder(KEY_MAX_LENGTH), method, (char *) path), callback);
}

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
      DEBUG(Serial.println(F("client wait timeout")));
      localClient->flush();
      localClient->stop();
    }
    return;
  }
  // got new client data, reset wait_to to 0
  wait_to = 0;


  // Update the timeout for each data read to ensure that the total timeout is WIFI_CONNECTION_TIMEOUT.
  unsigned int timeout = millis()+WIFI_CONNECTION_TIMEOUT;


  char *buffer = headerBuffer;
  size_t length = 0;
  while (localClient->dataAvailable()&&millis()<timeout) {
    if (length >= headerBufferSize) {
      localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
      // Get a new client to indicate that the previous client is no longer needed.
      localClient = localServer.acceptClient();
      return;
    }

    size_t read = localClient->readBytesUntil('\n', &buffer[length], min((int) (headerBufferSize - length - 1), headerBufferSize));
    char rc = buffer[length];
    length += read;
    buffer[length++] = '\n';
    if(read==1 && rc=='\r') { break; }
  }
  DEBUG(Serial.printf((char *) F("Finished reading data from client. Request line + headers were %d bytes\n"), length);)
  buffer[length] = 0;

  // Make sure that the headers were fully read into the buffer.
  if (strncmp_P(&buffer[length - 4], (char *) F("\r\n\r\n"), 4) != 0) {
    DEBUG(Serial.println(F("The request headers were not fully read into the buffer."));)
    localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
    return;
  }

  //Serial.println(F("Parsing request"));
  Request request(buffer, length, false);

  char *bodyBuffer = NULL;
  // If the request was valid, read the body and add it to the Request object.
  if (request.getType() > INVALID) {
    char *contentLengthString = request.getHeader(F("content-length"));
    // If the header was not specified, the message has no body.
    if (contentLengthString != nullptr) {
      long contentLength = String(contentLengthString).toInt();
      // If the header specifies a length of 0 or could not be parsed, the message has no body.
      if (contentLength > 0) {
        // Read the body from the client.
        bodyBuffer = new char[contentLength];
        size_t bodyLength = 0;
        timeout = millis()+WIFI_CONNECTION_TIMEOUT;
        while (localClient->dataAvailable() && millis()<timeout) {
          size_t read = localClient->readBytes(&bodyBuffer[bodyLength], min((int) (contentLength - bodyLength), 1024));
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
  res.enableStream([this](const char *buffer, size_t length, bool streaming) -> void {
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
    DEBUG(Serial.printf("Sent response, %d bytes\n", res.getTotalLength());)
  } else {
    localClient->print(F("HTTP/1.1 500 OTF error\r\nResponse string could not be built\r\n"));
    DEBUG(Serial.println(F("An error occurred while building the response string."));)
  }

  // Properly close the client connection.
  localClient->flush();
  localClient->stop();

  // Get a new client to indicate that the previous client is no longer needed.
  localClient = localServer.acceptClient();
  if (localClient) {
    DEBUG(Serial.println(F("Accepted new client"));)
    wait_to = millis()+WIFI_CONNECTION_TIMEOUT;
  }

  DEBUG(Serial.println(F("Finished handling request"));)
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
      DEBUG(Serial.println(F("Websocket connection closed"));)
      if (cloudStatus == CONNECTED) {
        // Make sure the cloud status is only set to disconnected if it was previously connected.
        setCloudStatus(DISCONNECTED);
      }
      break;
    }

    case WSEvent_CONNECTED: {
      DEBUG(Serial.println(F("Websocket connection opened"));)
      setCloudStatus(CONNECTED);
      break;
    }

    case WSEvent_PING: {
      DEBUG(Serial.println(F("Received a ping from the server"));)
      break;
    }

    case WSEvent_PONG: {
      DEBUG(Serial.println(F("Received a pong from the server"));)
      break;
    }

    case WSEvent_TEXT: {
      #define PREFIX_LENGTH 5
      #define ID_LENGTH 4
      // Length of the prefix, request ID, carriage return, and line feed.
      #define HEADER_LENGTH PREFIX_LENGTH + ID_LENGTH + 2

      char *message_data = (char*) payload;

      if (strncmp_P(message_data, (char *) F("FWD: "), PREFIX_LENGTH) == 0) {
        DEBUG(Serial.println(F("Message is a forwarded request."));)
        char *requestId = &message_data[PREFIX_LENGTH];
        // Replace the assumed carriage return with a null character to terminate the ID string.
        requestId[ID_LENGTH] = '\0';

        Request request(&message_data[HEADER_LENGTH], length - HEADER_LENGTH, true);
        Response res = Response();
        // Make response stream to websocket
        res.enableStream([this] (const char *buffer, size_t length, bool streaming) -> void {
          // If the websocket is not already streaming, start streaming.
          if (!streaming) {
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
          DEBUG(Serial.printf("Sent response, %d bytes\n", res.getTotalLength());)
        } else {
          DEBUG(Serial.println(F("An error occurred building response string"));)
          StringBuilder builder(100);
          builder.bprintf(F("RES: %s\r\n%s"), requestId,
                          F("HTTP/1.1 500 Internal Error\r\n\r\nAn internal error occurred"));
          if (!builder.isValid()) {
            DEBUG(Serial.println(F("Builder is not valid"));)
            return;
          }
        }
      } else {
        DEBUG(Serial.println(F("Websocket message does not start with the correct prefix."));)
      }
      break;
    }
    
    default: {
      DEBUG(Serial.printf((char *) F("Received unsupported websocket event of type %d\n"), type);)
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
  DEBUG(Serial.printf((char *) F("Attempting to route request to path '%s'\n"), req.getPath());)
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
    DEBUG(Serial.println(F("Found callback"));)
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
