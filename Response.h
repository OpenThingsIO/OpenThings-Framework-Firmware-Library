#ifndef OTF_RESPONSE_H
#define OTF_RESPONSE_H

#include "StringBuilder.hpp"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdint.h>
#include <functional>
#include <stdarg.h>
#endif

// The maximum possible size of response messages.
#define RESPONSE_BUFFER_SIZE 4096

namespace OTF {

  class Response : public StringBuilder {
    friend class OpenThingsFramework;

  private:
    enum ResponseStatus {
      CREATED,
      STATUS_WRITTEN,
      HEADERS_WRITTEN,
      BODY_WRITTEN
    };
    ResponseStatus responseStatus = CREATED;

    Response() : StringBuilder(RESPONSE_BUFFER_SIZE) {}


  public:
    static const size_t MAX_RESPONSE_LENGTH = RESPONSE_BUFFER_SIZE;

    /** Writes the status code/message to the response. This must be called before writing the headers or body. */
    #if defined(ARDUINO)
    void writeStatus(uint16_t statusCode, const String &statusMessage);
    void writeStatus(uint16_t statusCode, const __FlashStringHelper *const statusMessage);
    #else
    void writeStatus(uint16_t statusCode, const char *statusMessage);
    #endif


    /** Writes the status code to the response. This must be called before writing the headers or body. */
    void writeStatus(uint16_t statusCode);

    /** Sets a response header. If called multiple times with the same header name, the header will be specified
     * multiple times to create a list. This function must not be called before the status has been written or after
     * the body has been written.
     */
    #if defined(ARDUINO)
    void writeHeader(const __FlashStringHelper *const name, const __FlashStringHelper *const value);
    void writeHeader(const __FlashStringHelper *const name, int value);
    #else
    void writeHeader(const char *name, const char *const value);
    void writeHeader(const char *name, int value);
    #endif

    /**
     * Calls sprintf to write a chunk of data to the response body. This method may only be called after any desired
     * headers have been set.
     * @param format The format string to pass to sprintf.
     * @param ... The format arguments to pass to sprintf.
     */
    void writeBodyChunk(const char *format, ...);
    void writeBodyData(const char *data, size_t max_length);

#if defined(ARDUINO)
    void writeBodyChunk(const __FlashStringHelper *const format, ...);
    void writeBodyData(const __FlashStringHelper *const data, size_t max_length);
#endif
  };
}// namespace OTF
#endif
