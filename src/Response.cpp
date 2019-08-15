#include "LinkedMap.h"
#include "Response.h"

#include <Arduino.h>

using namespace OTF;

Response::Response(uint16_t statusCode, const String &body) {
  this->statusCode = statusCode;
  this->body = body;
}

void Response::setHeader(char *name, const char *value) {
  headers.add(name, value);
}

void Response::setHeader(const __FlashStringHelper *name, const char *value) {
  headers.add(name, value);
}

char *Response::toString() {
  // Return the string if it has already been built.
  if (builder.getLength() != 0) {
    if (builder.isValid()) {
      return builder.toString();
    } else {
      return nullptr;
    }
  }

  // Write the response line.
  builder.printf((char *) F("HTTP/1.1 %d No message\r\n"), statusCode);

  // Write the response headers.
  Serial.println(F("Writing headers"));
  OTF::LinkedMapNode<const char *> *node = headers.head;
  while (node != nullptr) {
    builder.printf((char *) F("%s: %s\r\n"), node->key, node->value);
    node = node->next;
  }

  // Write the response body.
  Serial.println(F("Writing body"));
  builder.printf((char *) F("\r\n%s"), body.c_str());
  Serial.println(F("Wrote body"));

  if (!builder.isValid()) {
    return nullptr;
  }

  return builder.toString();
}
