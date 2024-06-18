#include "StringBuilder.hpp"

using namespace OTF;

StringBuilder::StringBuilder(size_t maxLength) {
  this->maxLength = maxLength;
  buffer = new char[maxLength];
}

StringBuilder::~StringBuilder() {
  delete buffer;
}

void StringBuilder::bprintf(char *format, va_list args) {
  // Don't do anything if the buffer already contains invalid data.
  if (!valid) {
    return;
  }

  size_t res = vsnprintf(&buffer[length], maxLength - length, format, args);


  if (stream_write && ((res >= maxLength) || (length + res >= maxLength))) {
    // If in streaming mode flush the buffer and continue writing if the data doesn't fit.
    stream_write(buffer, length, streaming);
    stream_flush();
    clear();
    res = vsnprintf(&buffer[length], maxLength - length, format, args);
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

void StringBuilder::bprintf(char *const format, ...) {
  va_list args;
  va_start(args, format);
  bprintf(format, args);
  va_end(args);
}

void StringBuilder::bprintf(const __FlashStringHelper *const format, va_list args) {
  bprintf((char *) format, args);
}

void StringBuilder::bprintf(const __FlashStringHelper *const format, ...) {
  va_list args;
  va_start(args, format);
  bprintf(format, args);
  va_end(args);
}

size_t StringBuilder::write(const char *data, size_t data_length) {
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
      if (stream_write) {
        stream_write(buffer, length, streaming);
        stream_flush();
        clear();
      } else {
        // If the buffer is full and there is no stream to write to, the builder is invalid.
        valid = false;
        return -1;
      }
    } else {
      // Copy the data to the buffer.
      memcpy(&buffer[length], &data[write_index], write_length);
      length += write_length;
      totalLength += write_length;
      write_index += write_length;
      // Null-terminate the buffer.
      buffer[length] = '\0';
    }
  }

  return data_length;
}

void StringBuilder::enableStream(stream_write_t write, stream_flush_t flush, stream_end_t end) {
  stream_write = write;
  stream_flush = flush;
  stream_end = end;
}

bool StringBuilder::end() {
  if (stream_end) {
    stream_write(buffer, length, streaming);
    stream_end();
    return true;
  }
  return false;
}

char *StringBuilder::toString() const {
  return &buffer[0];
}

size_t StringBuilder::getLength() const {
  return length;
}

bool StringBuilder::isValid() {
  return valid;
}

void StringBuilder::clear() {
  length = 0;
  buffer[0] = '\0';
  valid = true;
}

size_t StringBuilder::getMaxLength() const {
  return maxLength;
}

size_t StringBuilder::getTotalLength() const {
  return totalLength;
}
