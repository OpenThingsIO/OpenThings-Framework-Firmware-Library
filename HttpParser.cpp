#include "HttpParser.h"

#include <stdint.h>

using namespace OTF;

ContentLengthResult OTF::parseContentLength(const char *value, size_t maximum, size_t &result) {
  result = 0;
  if (value == nullptr) return CONTENT_LENGTH_INVALID;

  while (*value == ' ' || *value == '\t') value++;
  if (*value == '\0') return CONTENT_LENGTH_INVALID;

  size_t parsed = 0;
  bool tooLarge = false;
  while (*value >= '0' && *value <= '9') {
    size_t digit = (size_t)(*value - '0');
    if (parsed > (SIZE_MAX - digit) / 10) return CONTENT_LENGTH_INVALID;
    parsed = parsed * 10 + digit;
    if (parsed > maximum) tooLarge = true;
    value++;
  }

  while (*value == ' ' || *value == '\t') value++;
  if (*value != '\0') return CONTENT_LENGTH_INVALID;
  if (tooLarge) return CONTENT_LENGTH_TOO_LARGE;

  result = parsed;
  return CONTENT_LENGTH_VALID;
}
