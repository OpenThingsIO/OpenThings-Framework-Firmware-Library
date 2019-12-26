#include "OpenThingsFramework.h"
#include "StringBuilder.h"

// The size of the buffer to store incoming requests in. Larger requests will be discarded.
#define REQUEST_BUFFER_SIZE 2048
// TODO This doesn't work because the macro is redefined in WebSockets.h
#define WEBSOCKETS_MAX_DATA_SIZE REQUEST_BUFFER_SIZE
#define WIFI_CONNECTION_TIMEOUT 1000
/* How often to try to reconnect to the websocket if the connection is lost. Each reconnect attempt is blocking and has
 * a 5 second timeout.
 */
#define WEBSOCKET_RECONNECT_INTERVAL 30000

using namespace OTF;

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, const String &webSocketHost, uint16_t webSocketPort,
                                         const String &deviceKey) : server(webServerPort) {
  Serial.println("Instantiating OTF...");
  missingPageCallback = defaultMissingPageCallback;
  server.begin();
  wifiClient = server.available();

  Serial.println(F("Initializing websocket..."));
  webSocket.begin(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
  Serial.println(F("Initialized websocket"));

  // Wrap the member function in a static function.
  webSocket.onEvent([this](WStype_t type, uint8_t *payload, size_t length) -> void {
      webSocketCallback(type, payload, length);
  });

  webSocket.setReconnectInterval(WEBSOCKET_RECONNECT_INTERVAL);
  // Ping the server every 15 seconds with a timeout of 5 seconds, and treat 1 missed ping as a lost connection.
  webSocket.enableHeartbeat(15000, 5000, 1);
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
  if (!wifiClient) {
    // If there isn't currently a pending client, check if one is available.
    wifiClient = server.available();

    // If a client wasn't available from the server, exit the local server loop.
    if (!wifiClient) {
      return;
    }

    wifiClient.setTimeout(WIFI_CONNECTION_TIMEOUT);
  }

  if (!wifiClient.available()) {
    // If data isn't available from the client yet, exit the local server loop and check again next iteration.
    return;
  }

  // Read the full request from the client.
  char buffer[REQUEST_BUFFER_SIZE];
  size_t length = 0;
  while (wifiClient.available()) {
    if (length >= REQUEST_BUFFER_SIZE) {
      Serial.println(F("Request buffer is full, aborting"));
      wifiClient.print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));

      // Get a new client to indicate that the previous client is no longer needed.
      wifiClient = server.available();
      return;
    }

    size_t read = wifiClient.readBytes(&buffer[length], min((int) (REQUEST_BUFFER_SIZE - length), 1024));
    length += read;
  }
  Serial.printf((char *) F("Finished reading data from client. Message was %d bytes\n"), length);

  Serial.println(F("Parsing request"));
  Request request(buffer, length);

  Serial.println(F("Filling response"));
  Response res = Response();
  fillResponse(request, res);

  Serial.println(F("Sending response"));
  if (res.isValid()) {
    char *responseString = res.toString();
    Serial.printf((char *) F("Response message is: %s\n"), responseString);
    wifiClient.print(responseString);
  } else {
    wifiClient.print(F("HTTP/1.1 500 OTF error\r\nResponse string could not be built\r\n"));
    Serial.println(F("An error occurred while building the response string."));
  }

  // Get a new client to indicate that the previous client is no longer needed.
  wifiClient = server.available();

  Serial.println(F("Finished handling request"));
}

void OpenThingsFramework::loop() {
  localServerLoop();
  webSocket.loop();
}

void OpenThingsFramework::webSocketCallback(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println(F("Disconnected from websocket"));
      break;
    case WStype_CONNECTED:
      Serial.println(F("Connected to websocket"));
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

        Request request(&message[HEADER_LENGTH], length - HEADER_LENGTH);
        Response res = Response();
        res.bprintf(F("RES: %s\r\n"), requestId);
        fillResponse(request, res);

        if (res.isValid()) {
          webSocket.sendTXT(res.toString(), res.getLength());
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
  if (req.body == nullptr) {
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
