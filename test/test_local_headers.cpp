// Local-server request framing: physical header lines, the kept-header filter,
// overlong lines and incomplete input. Uses the real framework, request and
// response code with a fake transport.
#include "OpenThingsFramework.h"
#include <assert.h>
#include <stdio.h>
#include <string>

using namespace OTF;

WebsocketClient *testSocket = nullptr;
FakeLocalClient *OTF::nextLocalClient = nullptr;
bool failCloudAllocation = false;

uint32_t millis() { static uint32_t now = 0; return ++now; }

static unsigned handled;
static std::string seenInm, seenBody;
static bool hadInm;

static void route(const Request &req, Response &res) {
  ++handled;
  const char *inm = req.getHeader(F("if-none-match"));
  hadInm = inm != nullptr;
  seenInm = inm ? inm : "";
  seenBody = req.getBody() ? std::string(req.getBody(), req.getBodyLength()) : "";
  res.writeStatus(200, F("OK"));
  res.writeBodyData("ok", 2);
}

struct Result { std::string output; unsigned handled; bool hadInm; std::string inm, body; };

static Result run(OpenThingsFramework &otf, const std::string &input) {
  FakeLocalClient client;
  client.input = input;
  nextLocalClient = &client;
  handled = 0; hadInm = false; seenInm.clear(); seenBody.clear();
  otf.loop(false);
  return {client.output, handled, hadInm, seenInm, seenBody};
}

// A header line of exactly `total` bytes before its '\n' (including the '\r').
static std::string line(const std::string &name, size_t total) {
  std::string s = name + ": ";
  assert(total > s.size() + 1);
  s.append(total - s.size() - 1, 'x');
  return s + "\r\n";
}

static const char *kGet = "GET /test HTTP/1.1\r\n";
static const char *kTag = "If-None-Match: \"0123456789abcdef\"\r\n";

int main() {
  OpenThingsFramework otf(0);
  otf.on("/test", route);

  // Baseline: the kept header reaches the handler, names lowercased.
  Result r = run(otf, std::string(kGet) + kTag + "\r\n");
  assert(r.handled == 1 && r.hadInm && r.inm.find("\"0123456789abcdef\"") != std::string::npos);

  // Lines around the 128-byte line buffer (127 stored bytes) never hide or
  // invent a following header. 127 is the case that previously left its '\n'
  // unread and ended header parsing early.
  for (size_t total = 120; total <= 300; ++total) {
    r = run(otf, std::string(kGet) + line("X-Pad", total) + kTag + "\r\n");
    assert(r.handled == 1 && r.hadInm);
    r = run(otf, std::string(kGet) + line("User-Agent", total) + "\r\n");
    assert(r.handled == 1 && !r.hadInm);
  }

  // A long header whose second buffer-sized chunk looks like a header is one
  // physical line: neither If-None-Match nor Content-Length may be taken from it.
  for (size_t pad = 120; pad <= 140; ++pad) {
    std::string fake = "X-Pad: " + std::string(pad - 7, 'a');
    r = run(otf, std::string(kGet) + fake + "If-None-Match: \"0123456789abcdef\"\r\n\r\n");
    assert(r.handled == 1 && !r.hadInm);
    r = run(otf, std::string("POST /test HTTP/1.1\r\n") + fake + "Content-Length: 5\r\n\r\nhello");
    assert(r.handled == 1 && r.body.empty());
  }

  // An If-None-Match line that does not fit is skipped, never truncated.
  std::string longInm = "If-None-Match: " + std::string(120, 'a') + ", \"0123456789abcdef\"\r\n";
  r = run(otf, std::string(kGet) + longInm + "\r\n");
  assert(r.handled == 1 && !r.hadInm);
  // The longest line that fits (127 bytes) is kept whole.
  std::string fits = "If-None-Match: \"0123456789abcdef\", " ;
  fits.append(127 - fits.size() - 1, 'b');
  r = run(otf, std::string(kGet) + fits + "\r\n\r\n");
  assert(r.handled == 1 && r.hadInm && r.inm.size() >= 100);

  // Short lines never compare past their end (ASan).
  r = run(otf, std::string(kGet) + "If\r\nC\r\n\r\n");
  assert(r.handled == 1 && !r.hadInm);
  // Bare-LF framing is read to the end of the headers; the request parser
  // itself requires CRLF (unchanged), so only completion is checked here.
  r = run(otf, std::string("GET /test HTTP/1.1\nIf-None-Match: \"0123456789abcdef\"\n\n"));
  assert(r.output.find("408") == std::string::npos);

  // Content-Length still drives the body.
  r = run(otf, std::string("POST /test HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello"));
  assert(r.handled == 1 && r.body == "hello");

  // Incomplete headers time out instead of dispatching a partial request.
  r = run(otf, std::string(kGet) + kTag);
  assert(r.handled == 0 && r.output.find("408") != std::string::npos);
  r = run(otf, std::string(kGet) + "X-Pad: " + std::string(300, 'a'));
  assert(r.handled == 0 && r.output.find("408") != std::string::npos);
  r = run(otf, "GET /test HTTP/1.1");
  assert(r.handled == 0 && r.output.find("408") != std::string::npos);

  // A request line longer than the header buffer is rejected, not truncated.
  // Its headers are read first, so no unread input resets the connection.
  {
    FakeLocalClient client;
    client.input = "GET /test?v=" + std::string(HEADERS_BUFFER_SIZE, 'q') + " HTTP/1.1\r\n" + kTag + "X: y\r\n\r\n";
    nextLocalClient = &client; handled = 0;
    otf.loop(false);
    assert(handled == 0 && client.output.find("414") != std::string::npos);
    assert(client.offset == client.input.size());
  }
  // One that fits is still accepted.
  r = run(otf, "GET /test?v=" + std::string(HEADERS_BUFFER_SIZE - 64, 'q') + " HTTP/1.1\r\n\r\n");
  assert(r.handled == 1);

  printf("Local header framing: line buffer boundaries, fragments, kept headers, timeouts and 414 passed\n");
}
