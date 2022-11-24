#include "OpenThingsFramework.h"
#include "config.h"

#include <Arduino.h>
#include <WiFiServer.h>

OTF::OpenThingsFramework *otf = nullptr;

void uptime(const OTF::Request &req, OTF::Response &res) {
  res.writeStatus(200, "OK");
  res.writeHeader(F("content-type"), F("text/plain"));
  bool useMicros = req.getQueryParameter(F("useMicros")) != nullptr;
  const __FlashStringHelper *units = useMicros ? F("microseconds") : F("milliseconds");
  res.writeBodyChunk(F("The server has been up for %d %s"), useMicros ? micros() : millis(), units);
}

void memoryUsage(const OTF::Request &req, OTF::Response &res) {
  res.writeStatus(200, "OK");
  res.writeHeader(F("content-type"), F("text/plain"));
  res.writeBodyChunk(F("The free heap size is %d bytes"), ESP.getFreeHeap());
}

void logMessage(const OTF::Request &req, OTF::Response &res) {
  Serial.print(F("Received message body: "));
  Serial.println(req.getBody());
  res.writeStatus(200, "OK");
  res.writeHeader(F("content-type"), F("text/plain"));
  res.writeBodyChunk(F("The message has been logged to the serial monitor"));
}

void missingPage(const OTF::Request &req, OTF::Response &res) {
  res.writeStatus(404, F("Not found"));
  res.writeHeader(F("content-type"), F("text/plain"));
  res.writeBodyChunk(F("That page does not exist"));
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to WiFi network...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("Connected");

  otf = new OTF::OpenThingsFramework(80, WEBSOCKET_HOST, WEBSOCKET_PORT, DEVICE_KEY, false);
  Serial.println("Created OTF object");

  otf->on(F("/uptime"), uptime);
  otf->on(F("/memory"), memoryUsage);
  otf->on(F("/log"), logMessage, OTF::HTTP_POST);
  otf->on(F("/log"), logMessage, OTF::HTTP_PUT);
  otf->onMissingPage(missingPage);
  Serial.println(F("Finished setup"));
}

void loop() {
  // Analogous to calling "server->handleClient".
  otf->loop();
}
