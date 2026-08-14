#include "StringBuilder.hpp"

#include <assert.h>
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
  return 0;
}
