#include "HttpParser.h"

#include <assert.h>
#include <stddef.h>

using namespace OTF;

static void expectValid(const char *value, size_t maximum, size_t expected) {
  size_t result = 123;
  assert(parseContentLength(value, maximum, result) == CONTENT_LENGTH_VALID);
  assert(result == expected);
}

static void expectResult(const char *value, size_t maximum, ContentLengthResult expected) {
  size_t result = 123;
  assert(parseContentLength(value, maximum, result) == expected);
}

int main() {
  expectValid("0", 8192, 0);
  expectValid("8192", 8192, 8192);
  expectValid("\t42 ", 8192, 42);

  expectResult(nullptr, 8192, CONTENT_LENGTH_INVALID);
  expectResult("", 8192, CONTENT_LENGTH_INVALID);
  expectResult("-1", 8192, CONTENT_LENGTH_INVALID);
  expectResult("+1", 8192, CONTENT_LENGTH_INVALID);
  expectResult("1x", 8192, CONTENT_LENGTH_INVALID);
  expectResult("1 2", 8192, CONTENT_LENGTH_INVALID);
  expectResult("184467440737095516160", 8192, CONTENT_LENGTH_INVALID);
  expectResult("8193", 8192, CONTENT_LENGTH_TOO_LARGE);
  return 0;
}
