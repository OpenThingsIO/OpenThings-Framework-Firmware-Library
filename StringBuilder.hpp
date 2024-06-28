#ifndef OTF_STRINGBUILDER_H
#define OTF_STRINGBUILDER_H

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdint.h>
#include <functional>
#define sprintf_P sprintf
#define strncpy_P strncpy
#define strcmp_P strcmp
#define strncmp_P strncmp
#define strlen_P strlen

#define F(x) x
#define String char *

 #define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#endif

namespace OTF {
  typedef std::function<void(const char *data, size_t length, bool streaming)> stream_write_t;
  typedef std::function<void()> stream_flush_t;
  typedef std::function<void()> stream_end_t;

  /**
   * Wraps a buffer to build a string with repeated calls to sprintf. If any of calls to sprintf cause an error (such
   * as exceeding the size of the internal buffer), the error will be silently swallowed and the StringBuilder will be
   * marked as invalid. This means that any error checking can occur after the entire string has been built instead of
   * a check being required after each individual call to sprintf.
   */
  class StringBuilder {
  private:
    size_t maxLength;
    char *buffer;
    size_t length = 0;
    size_t totalLength = 0;

    stream_write_t stream_write = nullptr;
    stream_flush_t stream_flush = nullptr;
    stream_end_t stream_end = nullptr;
    bool streaming = false;
    bool first_message = true;

    /**
     * Internal write function
     */
    size_t _write(const char *data, size_t data_length, bool use_pgm);

  protected:
    bool valid = true;

  public:
    explicit StringBuilder(size_t maxLength);

    ~StringBuilder();
    /**
     * Inserts a string into the buffer at the current position using the same formatting rules as printf. If the operation
     * would cause the buffer length to be exceeded or some other error occurs, the StringBuilder will be marked as invalid.
     * @param format The format string to pass to sprintf.
     * @param ... The format arguments to pass to sprintf.
     */
    void bprintf(char *format, va_list args);

    void bprintf(char *format, ...);

#if defined(ARDUINO)
    void bprintf(const __FlashStringHelper *const format, va_list args);
    void bprintf(const __FlashStringHelper *const format, ...);
#endif

    /**
     * Raw Write to buffer
     */
    size_t write(const char *data, size_t length);


    #if defined(ARDUINO)
    /**
     * Raw Write to buffer from PROGMEM
     */
    size_t write_P(const __FlashStringHelper *const data, size_t length);
    #endif

    /**
     * Enables streaming mode for the StringBuilder.
     */
    void enableStream(stream_write_t write, stream_flush_t flush, stream_end_t end);

    /**
     * @brief 
     * Flushes the buffer and ends streaming mode.
     */
    bool end();

    /**
     * Returns the null-terminated represented string stored in the underlying buffer.
     * @return The null-terminated represented string stored in the underlying buffer.
     */
    char *toString() const;

    size_t getLength() const;

    /**
     * Returns a boolean indicating if the string was built successfully without any errors. If false, the behavior
     * of toString() is undefined, and the string it returns (which may or may not be null terminated) should NOT
     * be used.
     */
    bool isValid();

    /**
     * Clears the buffer and resets the StringBuilder to a valid state.
     */
    void clear();

    /**
     * Returns the maximum length of the buffer.
     */
    size_t getMaxLength() const;

    /**
     * Total length of the string built so far.
     */
    size_t getTotalLength() const;
  };
}// namespace OTF

#endif
