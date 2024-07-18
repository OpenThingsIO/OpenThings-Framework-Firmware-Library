#include "Response.h"

using namespace OTF;

#if defined(ARDUINO)
void Response::writeStatus(uint16_t statusCode, const String &statusMessage) {
  if (responseStatus > CREATED) {
    valid = false;
    return;
  }
  responseStatus = STATUS_WRITTEN;

  bprintf(F("HTTP/1.1 %d %s\r\n"), statusCode, statusMessage.c_str());
}

void Response::writeStatus(uint16_t statusCode, const __FlashStringHelper *const statusMessage) {
  if (responseStatus > CREATED) {
    valid = false;
    return;
  }
  responseStatus = STATUS_WRITTEN;

  bprintf(F("HTTP/1.1 %d %s\r\n"), statusCode, statusMessage);
}
#else
void Response::writeStatus(uint16_t statusCode, const char *statusMessage) {
  if (responseStatus > CREATED) {
    valid = false;
    return;
  }
  responseStatus = STATUS_WRITTEN;

  bprintf(F("HTTP/1.1 %d %s\r\n"), statusCode, statusMessage);
}
#endif

void Response::writeStatus(uint16_t statusCode) {
  if (responseStatus > CREATED) {
    valid = false;
    return;
  }
  responseStatus = STATUS_WRITTEN;

  writeStatus(statusCode, F("No message"));
}

/*void Response::writeHeader(char *const name, char *const value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %s\r\n"), name, value);
}

void Response::writeHeader(const __FlashStringHelper *const name, char *const value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %s\r\n"), name, value);
}*/

#if defined(ARDUINO)
void Response::writeHeader(const __FlashStringHelper *const name, int value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %d\r\n"), name, value);
}

void Response::writeHeader(const __FlashStringHelper *const name, const __FlashStringHelper *const value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %s\r\n"), name, value);
}
#else
void Response::writeHeader(const char *name, int value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %d\r\n"), name, value);
}

void Response::writeHeader(const char *name, const char *value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %s\r\n"), name, value);
}
#endif

void Response::writeBodyChunk(char *const format, ...) {
  if (responseStatus < STATUS_WRITTEN) {
    valid = false;
    return;
  }
  if (responseStatus != BODY_WRITTEN) {
    bprintf((char *) "\r\n");
    responseStatus = BODY_WRITTEN;
  }

  va_list args;
  va_start(args, format);
  bprintf(format, args);
  va_end(args);
}

#if defined(ARDUINO)
void Response::writeBodyChunk(const __FlashStringHelper *const format, ...) {
  if (responseStatus < STATUS_WRITTEN) {
    valid = false;
    return;
  }
  if (responseStatus != BODY_WRITTEN) {
    bprintf((char *) "\r\n");
    responseStatus = BODY_WRITTEN;
  }

  va_list args;
  va_start(args, format);
  bprintf(format, args);
  va_end(args);
}
#endif

void Response::writeBodyData(const char *data, size_t length) {
  if (responseStatus < STATUS_WRITTEN) {
    valid = false;
    return;
  }
  if (responseStatus != BODY_WRITTEN) {
    bprintf((char *) "\r\n");
    responseStatus = BODY_WRITTEN;
  }

  write(data, length);
}

#if defined(ARDUINO)
void Response::writeBodyData(const __FlashStringHelper *const data, size_t length) {
  if (responseStatus < STATUS_WRITTEN) {
    valid = false;
    return;
  }
  if (responseStatus != BODY_WRITTEN) {
    bprintf((char *) "\r\n");
    responseStatus = BODY_WRITTEN;
  }

  write_P(data, length);
}
#endif