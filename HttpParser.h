#ifndef OTF_HTTPPARSER_H
#define OTF_HTTPPARSER_H

#include <stddef.h>

namespace OTF {
  enum ContentLengthResult {
    CONTENT_LENGTH_VALID,
    CONTENT_LENGTH_INVALID,
    CONTENT_LENGTH_TOO_LARGE
  };

  /**
   * Parses a Content-Length field value without accepting signs, internal
   * whitespace, trailing non-whitespace characters, or integer overflow.
   */
  ContentLengthResult parseContentLength(const char *value, size_t maximum, size_t &result);
}

#endif
