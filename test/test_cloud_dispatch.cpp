#include "OpenThingsFramework.h"
#include <assert.h>
#include <stdlib.h>
#include <string>

using namespace OTF;

WebsocketClient *testSocket = nullptr;
FakeLocalClient *OTF::nextLocalClient = nullptr;
bool failCloudAllocation = false;
static unsigned handled = 0;
static OpenThingsFramework *framework = nullptr;
static std::function<void(const Request &, Response &)> handler;

uint32_t millis() { static uint32_t now = 0; return ++now; }

extern "C" void *__real_malloc(size_t);
extern "C" void *__wrap_malloc(size_t size) {
  if (failCloudAllocation) {
    failCloudAllocation = false;
    return nullptr;
  }
  return __real_malloc(size);
}

static void route(const Request &req, Response &res) {
  ++handled;
#if defined(ARDUINO)
  assert(!testSocket->inPoll);
#else
  assert(!req.isCloudRequest() || testSocket->inPoll);
#endif
  if (handler) handler(req, res);
  else {
    res.writeStatus(200, F("OK"));
    res.writeBodyChunk("value=%s", req.getQueryParameter("value"));
  }
}

static std::string frame(const std::string &id, const std::string &path = "/test?value=hello") {
  return "FWD: " + id + "\r\nGET " + path + " HTTP/1.1\r\n\r\n";
}

static void enqueue(const std::string &id) {
  testSocket->events.push_back({WSEvent_TEXT, frame(id)});
}

static void reset() {
  handled = 0;
  handler = nullptr;
  failCloudAllocation = false;
  testSocket->responses.clear();
  testSocket->events.clear();
  testSocket->afterCallback = nullptr;
  testSocket->onSend = nullptr;
  testSocket->onStream = nullptr;
  testSocket->failEnd = false;
  testSocket->failSend = false;
  testSocket->failStream = false;
  testSocket->drainAll = false;
  testSocket->closeCount = 0;
  testSocket->emit(WSEvent_CONNECTED);
}

int main() {
  OpenThingsFramework otf(0, "host", 80, "test-key", false);
  framework = &otf;
  otf.on("/test", route);

  // A destroyed callback payload remains valid in the deferred handler.
  testSocket->afterCallback = []() {
#if defined(ARDUINO)
    assert(handled == 0);
#else
    assert(handled == 1);
#endif
  };
  enqueue("0001");
  otf.loop();
  assert(handled == 1 && testSocket->responses.size() == 1);
  assert(testSocket->responses[0] == "RES: 0001\r\nHTTP/1.1 200 OK\r\n\r\nvalue=hello");

  reset();
  // Local requests retain priority, and allowCloud=false never polls.
  FakeLocalClient local;
  local.input = "GET /test?value=local HTTP/1.1\r\n\r\n";
  nextLocalClient = &local;
  enqueue("0002");
  unsigned polls = testSocket->pollCount;
  otf.loop(false);
  assert(handled == 1 && testSocket->pollCount == polls);
  assert(local.output.find("value=local") != std::string::npos);
  otf.loop(false);
  assert(testSocket->pollCount == polls);
  otf.loop();
  assert(handled == 2 && testSocket->responses.size() == 1);

  reset();
  // App-style bursts: Arduino drains one per loop; Linux handles all per poll.
  for (unsigned i = 0; i < 12; ++i) enqueue(std::to_string(1000 + i));
#if !defined(ARDUINO)
  testSocket->drainAll = true;
#endif
  for (unsigned i = 0; i < 12; ++i) otf.loop();
  assert(handled == 12 && testSocket->responses.size() == 12);
  assert(testSocket->closeCount == 0);

  reset();
  testSocket->onStream = []() {
    testSocket->emit(WSEvent_DISCONNECTED);
    testSocket->emit(WSEvent_CONNECTED);
  };
  enqueue("0018");
  otf.loop();
  assert(handled == 1 && testSocket->responses.empty());
  assert(testSocket->closeCount == 0);

  reset();
  // A handler cannot recursively poll or execute another request.
  handler = [](const Request &, Response &res) {
    unsigned pollsBefore = testSocket->pollCount;
    framework->loop();
    assert(pollsBefore == testSocket->pollCount);
    res.writeStatus(200, F("OK"));
  };
  enqueue("0003");
  otf.loop();
  assert(handled == 1 && testSocket->closeCount == 0);

#if defined(ARDUINO)
  reset();
  // Refused allocations return an ID-matched, standalone 503 without a handler.
  enqueue("0004");
  testSocket->afterCallback = []() { assert(handled == 0); };
  failCloudAllocation = true;
  otf.loop();
  assert(!failCloudAllocation && handled == 0);
  assert(testSocket->responses.size() == 1);
  assert(testSocket->responses[0].find("RES: 0004\r\nHTTP/1.1 503 ") == 0);
  assert(testSocket->closeCount == 0);

  reset();
  testSocket->events.push_back({WSEvent_TEXT, frame("0005") + std::string(WEBSOCKETS_MAX_DATA_SIZE, 'x')});
  otf.loop();
  assert(handled == 0 && testSocket->responses.size() == 1);
  assert(testSocket->responses[0].find("503 Service Unavailable") != std::string::npos);

  reset();
  // Defensive busy-slot behavior, even if a future backend batches frames.
  testSocket->drainAll = true;
  enqueue("0006");
  enqueue("0007");
  otf.loop();
  assert(handled == 1 && testSocket->responses.size() == 2);
  assert(testSocket->responses[0].find("RES: 0007\r\nHTTP/1.1 503 ") == 0);
  assert(testSocket->responses[1].find("RES: 0006\r\nHTTP/1.1 200 ") == 0);
  assert(testSocket->closeCount == 0);

  reset();
  // A queued request belongs only to its connection, never to the reconnect.
  testSocket->drainAll = true;
  enqueue("0008");
  testSocket->events.push_back({WSEvent_DISCONNECTED, ""});
  testSocket->events.push_back({WSEvent_CONNECTED, ""});
  otf.loop();
  assert(handled == 0 && testSocket->responses.empty());
#endif

  reset();
  // Disconnect cannot free the active payload or send its reply on a new session.
  handler = [](const Request &req, Response &res) {
    testSocket->emit(WSEvent_DISCONNECTED);
    testSocket->emit(WSEvent_CONNECTED);
    assert(strcmp(req.getQueryParameter("value"), "hello") == 0);
    res.writeStatus(200, F("OK"));
  };
  enqueue("0009");
  otf.loop();
  assert(handled == 1 && testSocket->responses.empty());
  assert(testSocket->closeCount == 0);

  reset();
  // Invalid unsent responses get one actual 500, without flushing invalid data.
  handler = [](const Request &, Response &res) {
    res.writeStatus(200, F("OK"));
    res.writeStatus(201, F("Invalid second status"));
  };
  enqueue("0010");
  otf.loop();
  assert(testSocket->responses.size() == 1);
  assert(testSocket->responses[0].find("RES: 0010\r\nHTTP/1.1 500 ") == 0);

  reset();
  // A streamed response spans buffers but remains one message with one ID.
  const std::string large(12000, 'x');
  handler = [&large](const Request &, Response &res) {
    res.writeStatus(200, F("OK"));
    res.writeBodyData(large.data(), large.size());
  };
  enqueue("0011");
  otf.loop();
  assert(testSocket->responses.size() == 1);
  assert(testSocket->responses[0] == "RES: 0011\r\nHTTP/1.1 200 OK\r\n\r\n" + large);

  reset();
  handler = [&large](const Request &, Response &res) {
    res.writeStatus(200, F("OK"));
    res.writeBodyData(large.data(), large.size());
    res.writeStatus(201, F("Invalid after partial reply"));
  };
  enqueue("0012");
  otf.loop();
#if defined(ARDUINO)
  assert(testSocket->responses.empty());
  assert(testSocket->closeCount == 1 && testSocket->reconnectOnClose);
#else
  assert(testSocket->responses.size() == 1 && testSocket->closeCount == 0);
  assert(testSocket->responses[0].find("500 Internal Server Error") == std::string::npos);
#endif

#if defined(ARDUINO)
  reset();
  // Failed end() leaves a stream open: close shallowly, then recover normally.
  testSocket->failEnd = true;
  enqueue("0013");
  otf.loop();
  assert(testSocket->closeCount == 1);
  testSocket->failEnd = false;
  testSocket->emit(WSEvent_CONNECTED);
  enqueue("0014");
  otf.loop();
  assert(handled == 2 && testSocket->responses.size() == 1);
  assert(testSocket->responses[0].find("RES: 0014\r\n") == 0);

  reset();
  testSocket->streaming = true;
  enqueue("0015");
  otf.loop();
  assert(handled == 0 && testSocket->responses.empty() && testSocket->closeCount == 1);

  reset();
  testSocket->events.push_back({WSEvent_TEXT, "FWD: bad\r\n"});
  otf.loop();
  assert(handled == 0 && testSocket->responses.empty() && testSocket->closeCount == 0);

  reset();
  testSocket->failStream = true;
  enqueue("0016");
  otf.loop();
  assert(testSocket->closeCount == 1 && testSocket->responses.empty());

  reset();
  testSocket->failSend = true;
  enqueue("0017");
  otf.loop();
  assert(testSocket->closeCount == 1 && testSocket->responses.empty());

  reset();
  testSocket->failSend = true;
  failCloudAllocation = true;
  enqueue("0019");
  otf.loop();
  assert(handled == 0 && testSocket->closeCount == 1);

  // Pending storage is also released if OTF is destroyed before dispatch.
  WebsocketClient *originalSocket = testSocket;
  {
    OpenThingsFramework pending(0, "host", 80, "test-key", false);
    testSocket->emit(WSEvent_TEXT, frame("0020"));
  }
  testSocket = originalSocket;
#endif
  return 0;
}
