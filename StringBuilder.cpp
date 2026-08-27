#include "StringBuilder.hpp"

#include <new>

using namespace OTF;

namespace {

int format_string(char *destination, size_t capacity, const char *format, va_list args) {
  va_list copy;
  va_copy(copy, args);
  int result = vsnprintf(destination, capacity, format, copy);
  va_end(copy);
  return result;
}

} // namespace

StringBuilder::StringBuilder(size_t maxLength) {
  this->maxLength = maxLength;
  if (maxLength == 0) {
    valid = false;
    return;
  }

  buffer = new (std::nothrow) char[maxLength];
  if (!buffer) {
    valid = false;
    return;
  }
  buffer[0] = '\0';
}

StringBuilder::~StringBuilder() {
  delete[] buffer;
}

void StringBuilder::vbprintf(const char *format, va_list args) {
  // Don't do anything if the buffer already contains invalid data.
  if (!valid) {
    return;
  }

  int result = format_string(&buffer[length], maxLength - length, format, args);
  if (result < 0) {
    valid = false;
    return;
  }
  size_t res = static_cast<size_t>(result);

  if (streaming && ((res >= maxLength) || (length + res >= maxLength))) {
    // If in streaming mode flush the buffer and continue writing if the data doesn't fit.
    stream_write(buffer, length, first_message);
    first_message = false;
    stream_flush();
    clear();
    result = format_string(&buffer[length], maxLength - length, format, args);
    if (result < 0) {
      valid = false;
      return;
    }
    res = static_cast<size_t>(result);
  }

  totalLength += res;
  length += res;

  // The builder is invalid if the string fits perfectly in the buffer since there wouldn't be room for the null terminator.
  if (length >= maxLength) {
    // snprintf will not allow more than the specified number of characters to be written to the buffer, so the length will be the buffer size.
    length = maxLength;
    valid = false;
  }
}

void StringBuilder::bprintf(const char *const format, ...) {
  va_list args;
  va_start(args, format);
  vbprintf(format, args);
  va_end(args);
}

#if defined(ARDUINO)
void StringBuilder::vbprintf(const __FlashStringHelper *const format, va_list args) {
  vbprintf((const char *) format, args);
}

void StringBuilder::bprintf(const __FlashStringHelper *const format, ...) {
  va_list args;
  va_start(args, format);
  vbprintf(format, args);
  va_end(args);
}
#endif

size_t StringBuilder::_write(const char *data, size_t data_length, bool use_pgm) {
  #if !defined(ARDUINO)
  (void)use_pgm;
  #endif
  if (!valid) {
    return -1;
  }

  size_t write_index = 0;

  while (write_index < data_length) {
    // Write as much data as possible to the buffer.
    size_t remaining = data_length - write_index;
    size_t write_length = maxLength - length - 1;

    if (write_length > remaining) {
      write_length = remaining;
    }

    // If the buffer is full, flush it and continue writing.
    if (write_length == 0) {
      if (streaming) {
        stream_write(buffer, length, first_message);
        first_message = false;
        stream_flush();
        clear();
      } else {
        // If the buffer is full and there is no stream to write to, the builder is invalid.
        valid = false;
        return -1;
      }
    } else {
      // Copy the data to the buffer.
      #if defined(ARDUINO)
      if (use_pgm) {
        memcpy_P(&buffer[length], &data[write_index], write_length);
      } else {
        memcpy(&buffer[length], &data[write_index], write_length);
      }
    #else 
    memcpy(&buffer[length], &data[write_index], write_length);
    #endif
      length += write_length;
      totalLength += write_length;
      write_index += write_length;
      // Null-terminate the buffer.
      buffer[length] = '\0';
    }
  }

  return data_length;
}

size_t StringBuilder::write(const char *data, size_t data_length) {
  return _write(data, data_length, false);
}

#if defined(ARDUINO)
size_t StringBuilder::write_P(const __FlashStringHelper *const data, size_t data_length) {
  return _write((const char *) data, data_length, true);
}
#endif

void StringBuilder::enableStream(stream_write_t write, stream_flush_t flush, stream_end_t end) {
  streaming = true;
  first_message = true;
  stream_write = write;
  stream_flush = flush;
  stream_end = end;
}

bool StringBuilder::end() {
  if (!stream_end) return false;

  if (buffer && stream_write) {
    stream_write(buffer, length, first_message);
    first_message = false;
    stream_end();
    return true;
  }

  // Even an invalid builder must terminate an established stream so the
  // remote peer is not left waiting for a response that can never arrive.
  stream_end();
  return false;
}

char *StringBuilder::toString() const {
  static char emptyString[] = "";
  return buffer ? buffer : emptyString;
}

size_t StringBuilder::getLength() const {
  return length;
}

bool StringBuilder::isValid() {
  return valid;
}

void StringBuilder::clear() {
  length = 0;
  if (buffer && maxLength > 0) {
    buffer[0] = '\0';
    valid = true;
  } else {
    valid = false;
  }
}

size_t StringBuilder::getMaxLength() const {
  return maxLength;
}

size_t StringBuilder::getTotalLength() const {
  return totalLength;
}
