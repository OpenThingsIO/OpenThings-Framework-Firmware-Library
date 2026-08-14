#include "ForwardedRequest.h"

#include <assert.h>
#include <string.h>

using namespace OTF;

static void expectInvalid(uint8_t *payload, size_t length) {
  ForwardedRequest request;
  request.requestId = reinterpret_cast<char *>(1);
  request.requestData = reinterpret_cast<char *>(1);
  request.requestLength = 1;
  assert(!parseForwardedRequest(payload, length, request));
  assert(request.requestId == nullptr);
  assert(request.requestData == nullptr);
  assert(request.requestLength == 0);
}

int main() {
  uint8_t shortFrame[] = "FWD: 1234\r\n";
  expectInvalid(nullptr, sizeof(shortFrame) - 1);
  for (size_t length = 0; length < 11; ++length) {
    expectInvalid(shortFrame, length);
  }

  uint8_t badPrefix[] = "BAD: 1234\r\nGET / HTTP/1.1\r\n\r\n";
  expectInvalid(badPrefix, sizeof(badPrefix) - 1);

  uint8_t badCarriageReturn[] = "FWD: 1234X\nGET / HTTP/1.1\r\n\r\n";
  expectInvalid(badCarriageReturn, sizeof(badCarriageReturn) - 1);

  uint8_t badLineFeed[] = "FWD: 1234\rXGET / HTTP/1.1\r\n\r\n";
  expectInvalid(badLineFeed, sizeof(badLineFeed) - 1);

  uint8_t emptyRequest[] = "FWD: ab12\r\n";
  ForwardedRequest empty;
  assert(parseForwardedRequest(emptyRequest, sizeof(emptyRequest) - 1, empty));
  assert(strcmp(empty.requestId, "ab12") == 0);
  assert(empty.requestData == reinterpret_cast<char *>(emptyRequest) + 11);
  assert(empty.requestLength == 0);

  uint8_t valid[] =
    "FWD: z9-!\r\nPOST /test HTTP/1.1\r\nContent-Length: 4\r\n\r\ndata";
  const size_t frameLength = sizeof(valid) - 1;
  ForwardedRequest request;
  assert(parseForwardedRequest(valid, frameLength, request));
  assert(strcmp(request.requestId, "z9-!") == 0);
  assert(strcmp(request.requestData,
                "POST /test HTTP/1.1\r\nContent-Length: 4\r\n\r\ndata") == 0);
  assert(request.requestLength == frameLength - 11);
  return 0;
}
