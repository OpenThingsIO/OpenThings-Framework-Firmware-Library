#include "Websocket.h"
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  WebsocketClient client;
  unsigned connected = 0;
  uint32_t start = millis(), maxPoll = 0;
  client.onEvent([&](WSEvent_t event, uint8_t *, size_t) {
    if (event == WSEvent_CONNECTED) ++connected;
    if (event == WSEvent_CONNECTED || event == WSEvent_DISCONNECTED)
      std::printf("event %u %d\n", uint32_t(millis()-start), int(event));
  });
  client.connect("127.0.0.1", std::atoi(argv[1]), "/test-no-token");
  client.setReconnectInterval(2000);
  client.enableHeartbeat(15000, 5000, 1);
  while (uint32_t(millis()-start) < 150000) {
    uint32_t before = millis();
    client.poll();
    uint32_t elapsed = millis()-before;
    if (elapsed > maxPoll) maxPoll = elapsed;
    usleep(1000);
  }
  client.close(false);
  std::printf("connections %u max_poll_ms %u\n", connected, maxPoll);
  return connected >= 6 ? 0 : 1;
}
