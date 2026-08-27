#include "StringBuilder.hpp"

#include <assert.h>
#include <limits>
#include <string>
#include <string.h>
#include <vector>

using namespace OTF;

int main() {
  StringBuilder normal(32);
  assert(normal.isValid());
  assert(strcmp(normal.toString(), "") == 0);
  normal.bprintf("value=%d", 42);
  assert(normal.isValid());
  assert(strcmp(normal.toString(), "value=42") == 0);

  char text[] = "text";
  StringBuilder pointerArgument(32);
  pointerArgument.bprintf("%s-%u", text, 7U);
  assert(pointerArgument.isValid());
  assert(strcmp(pointerArgument.toString(), "text-7") == 0);

  std::string formattedOutput;
  std::vector<bool> formattedFlags;
  size_t formattedFlushes = 0;
  bool formattedEnded = false;
  StringBuilder formattedStream(12);
  formattedStream.enableStream(
    [&formattedOutput, &formattedFlags](const char *data, size_t length, bool firstMessage) {
      formattedOutput.append(data, length);
      formattedFlags.push_back(firstMessage);
    },
    [&formattedFlushes]() { ++formattedFlushes; },
    [&formattedEnded]() { formattedEnded = true; });
  assert(formattedStream.write("prefix", 6) == 6);
  formattedStream.bprintf("value=%d", 42);
  assert(formattedStream.isValid());
  assert(formattedStream.end());
  assert(formattedOutput == "prefixvalue=42");
  assert(formattedFlags.size() == 2);
  assert(formattedFlags[0]);
  assert(!formattedFlags[1]);
  assert(formattedFlushes == 1);
  assert(formattedEnded);

  std::string rawOutput;
  std::vector<bool> rawFlags;
  StringBuilder rawStream(5);
  rawStream.enableStream(
    [&rawOutput, &rawFlags](const char *data, size_t length, bool firstMessage) {
      rawOutput.append(data, length);
      rawFlags.push_back(firstMessage);
    },
    []() {},
    []() {});
  assert(rawStream.write("abcdefghij", 10) == 10);
  assert(rawStream.end());
  assert(rawOutput == "abcdefghij");
  assert(rawFlags.size() == 3);
  assert(rawFlags[0]);
  assert(!rawFlags[1]);
  assert(!rawFlags[2]);

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
