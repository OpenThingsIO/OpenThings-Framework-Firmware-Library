#include "ForwardedRequest.h"

#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace {

const size_t PREFIX_LENGTH = 5;
const size_t ID_LENGTH = 4;
const size_t HEADER_LENGTH = PREFIX_LENGTH + ID_LENGTH + 2;

bool hasForwardedPrefix(const char *message) {
#if defined(ARDUINO)
  return strncmp_P(message, PSTR("FWD: "), PREFIX_LENGTH) == 0;
#else
  return memcmp(message, "FWD: ", PREFIX_LENGTH) == 0;
#endif
}

} // namespace

namespace OTF {

bool parseForwardedRequest(uint8_t *payload, size_t length, ForwardedRequest &request) {
  request = ForwardedRequest();
  if (!payload || length < HEADER_LENGTH) return false;

  char *message = reinterpret_cast<char *>(payload);
  if (!hasForwardedPrefix(message)) return false;
  if (message[PREFIX_LENGTH + ID_LENGTH] != '\r' ||
      message[PREFIX_LENGTH + ID_LENGTH + 1] != '\n') {
    return false;
  }

  request.requestId = message + PREFIX_LENGTH;
  request.requestData = message + HEADER_LENGTH;
  request.requestLength = length - HEADER_LENGTH;
  message[PREFIX_LENGTH + ID_LENGTH] = '\0';
  return true;
}

} // namespace OTF
