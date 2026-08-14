#include "Request.h"

#include <assert.h>
#include <string.h>

namespace OTF {

class RequestTestAccess {
public:
  static void expectValid(const char *encodedValue, const char *decodedValue) {
    char requestData[256];
    int length = snprintf(requestData, sizeof(requestData),
                          "GET /test?value=%s HTTP/1.1\r\n\r\n", encodedValue);
    assert(length > 0 && static_cast<size_t>(length) < sizeof(requestData));

    Request request(requestData, static_cast<size_t>(length), false);
    assert(request.getType() == NORMAL);
    assert(strcmp(request.getQueryParameter("value"), decodedValue) == 0);
  }

  static void expectInvalid(const char *encodedValue) {
    char requestData[256];
    int length = snprintf(requestData, sizeof(requestData),
                          "GET /test?value=%s HTTP/1.1\r\n\r\n", encodedValue);
    assert(length > 0 && static_cast<size_t>(length) < sizeof(requestData));

    Request request(requestData, static_cast<size_t>(length), false);
    assert(request.getType() == INVALID);
  }
};

} // namespace OTF

int main() {
  OTF::RequestTestAccess::expectValid("plain", "plain");
  OTF::RequestTestAccess::expectValid("a+b%20c%2F%7e", "a b c/~");
  OTF::RequestTestAccess::expectValid("%41%4a%4B", "AJK");

  OTF::RequestTestAccess::expectInvalid("%");
  OTF::RequestTestAccess::expectInvalid("%A");
  OTF::RequestTestAccess::expectInvalid("%GG");
  OTF::RequestTestAccess::expectInvalid("%0G");
  OTF::RequestTestAccess::expectInvalid("before%00after");
  return 0;
}
