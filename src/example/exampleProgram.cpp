#include "OpenThingsFramework.h"
#include "config.h"

#include <Arduino.h>
#include <WiFiServer.h>

extern "C" {
  #include "user_interface.h"
};


OTF::OpenThingsFramework *otf = nullptr;

OTF::Response uptime(const OTF::Request &req) {
  String body = "The server has been up for ";
  if (req.getQueryParameter(F("useMicros")) != nullptr) {
    body += String(micros()) + " microseconds";
  } else {
    body += String(millis()) + " milliseconds";
  }

  OTF::Response response(200, body);
  response.setHeader((char *) F("content-type"), (char *) F("text/plain"));
  return response;
}

OTF::Response memoryUsage(const OTF::Request &req) {
  String body = "The free heap size is " + String(system_get_free_heap_size());
  OTF::Response response(200, body);
  response.setHeader((char *) F("content-type"), (char *) F("text/plain"));
  return response;
}

OTF::Response logMessage(const OTF::Request &req) {
  std::string body = std::string(req.getBody(), req.getBodyLength());
  Serial.println(body.c_str());
  return OTF::Response(200, F("The message has been logged to the console"));
}

OTF::Response missingPage(const OTF::Request &req) {
  OTF::Response response(404, "That page does not exist.");
  response.setHeader((char *) F("content-type"), (char *) F("text/plain"));
  return response;
}

void setup() {
  Serial.begin(74880);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to WiFi network...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("Connected");

  otf = new OTF::OpenThingsFramework(80, WEBSOCKET_HOST, WEBSOCKET_PORT, DEVICE_KEY);
  Serial.println("Created OTF object");

  otf->on(F("/uptime"), uptime);
  otf->on(F("/memory"), memoryUsage);
  otf->on(F("/log"), logMessage, OTF::HTTP_POST);
  otf->onMissingPage(missingPage);
  Serial.println(F("Finished setup"));
}

void loop() {
  // Analogous to calling "server->handleClient".
  otf->loop();
}
