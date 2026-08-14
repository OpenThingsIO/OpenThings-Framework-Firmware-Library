#include "StringBuilder.hpp"

#include <assert.h>
#include <limits>
#include <string.h>

using namespace OTF;

int main() {
  StringBuilder normal(32);
  assert(normal.isValid());
  assert(strcmp(normal.toString(), "") == 0);
  normal.bprintf("value=%d", 42);
  assert(normal.isValid());
  assert(strcmp(normal.toString(), "value=42") == 0);

  StringBuilder empty(0);
  assert(!empty.isValid());
  assert(strcmp(empty.toString(), "") == 0);
  empty.bprintf("ignored");
  assert(empty.write("ignored", 7) == static_cast<size_t>(-1));
  assert(!empty.end());
  empty.clear();
  assert(!empty.isValid());
  assert(strcmp(empty.toString(), "") == 0);

  bool wroteInvalidStream = false;
  bool endedInvalidStream = false;
  empty.enableStream(
    [&wroteInvalidStream](const char *, size_t, bool) { wroteInvalidStream = true; },
    []() {},
    [&endedInvalidStream]() { endedInvalidStream = true; });
  assert(!empty.end());
  assert(!wroteInvalidStream);
  assert(endedInvalidStream);

  volatile size_t impossibleSize = std::numeric_limits<size_t>::max();
  StringBuilder allocationFailure(impossibleSize);
  assert(!allocationFailure.isValid());
  assert(strcmp(allocationFailure.toString(), "") == 0);
  allocationFailure.clear();
  assert(!allocationFailure.isValid());
  return 0;
}
