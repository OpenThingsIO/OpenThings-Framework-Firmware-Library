#include "Websocket.h"
#include <cassert>
#include <cstdio>

namespace ReconnectTest {
uint32_t now;
uint32_t connectDuration;
uint32_t cleanupDuration;
Peer peer;
std::vector<uint32_t> attempts;
std::vector<uint32_t> disconnects;
}

uint32_t millis() { return ReconnectTest::now; }

class Client : public WebsocketClient {
public:
  void tick() { advancePeer(); poll(); }
  void dropTcp() { tcpUp = false; }
  bool connected() const { return _client.status == WSC_CONNECTED; }
};

static void reset(ReconnectTest::Peer peer, uint32_t start = 60000,
                  uint32_t connectDuration = 0, uint32_t cleanupDuration = 0) {
  ReconnectTest::now = start;
  ReconnectTest::peer = peer;
  ReconnectTest::connectDuration = connectDuration;
  ReconnectTest::cleanupDuration = cleanupDuration;
  ReconnectTest::attempts.clear();
  ReconnectTest::disconnects.clear();
}

static void begin(Client &client, uint32_t interval = 5000) {
  client.connect("test.invalid", 80, "/socket");
  client.setReconnectInterval(interval);
}

static void run(Client &client, uint32_t duration) {
  uint32_t start = millis();
  while (static_cast<uint32_t>(millis()-start) < duration) {
    client.tick();
    ReconnectTest::now += 10;
  }
}

static uint32_t gap(size_t i) {
  return ReconnectTest::attempts[i]-ReconnectTest::attempts[i-1];
}

static void burst(ReconnectTest::Peer peer, uint32_t start = 60000) {
  reset(peer, start, 30);
  Client client;
  begin(client);
  run(client, 60000);
  assert(ReconnectTest::attempts.size() >= 10 && ReconnectTest::attempts.size() <= 12);
  for (size_t i=1; i<ReconnectTest::attempts.size(); ++i) assert(gap(i) >= 5000 && gap(i) <= 5100);
}

static void refusal(uint32_t connectDuration, uint32_t firstGap, uint32_t start = 60000) {
  reset(ReconnectTest::Peer::Refuse, start, connectDuration);
  Client client;
  begin(client);
  run(client, 900000);
  assert(ReconnectTest::attempts.size() >= 5);
  assert(gap(1) >= firstGap && gap(1) <= firstGap+20);
  assert(gap(2) == 60000 && gap(3) == 300000 && gap(4) == 300000);
}

int main() {
  using namespace ReconnectTest;
  burst(Peer::SilentBeforeUpgrade);
  burst(Peer::SilentAfterUpgrade);
  burst(Peer::SilentAfterUpgrade, 0xffffffffu-20000);
  burst(Peer::HttpReject);
  burst(Peer::CloseFrame);
  refusal(0, 15000);
  refusal(12000, 17000);
  refusal(12000, 17000, 0xffffffffu-14000);

  reset(Peer::Healthy, 2000);
  {
    Client client;
    begin(client);
    run(client, 10000);
    assert(attempts.size() == 1 && attempts[0] == 5000);
  }
  reset(Peer::Healthy);
  {
    Client client;
    begin(client);
    run(client, 100);
    assert(client.connected());
    uint32_t closed = now;
    client.close(true);
    run(client, 10000);
    assert(attempts.size() == 2 && attempts[1]-closed == 5000);
    assert(client.connected());
  }
  reset(Peer::Healthy);
  {
    Client client;
    begin(client);
    run(client, 100);
    uint32_t dropped = now;
    client.dropTcp();
    run(client, 10000);
    assert(attempts.size() == 2 && attempts[1]-dropped == 5000);
    assert(client.connected());
  }
  reset(Peer::HeartbeatTimeout);
  {
    Client client;
    begin(client);
    client.enableHeartbeat(100, 50, 1);
    run(client, 1000);
    assert(disconnects.size() == 1 && attempts.size() == 1);
    uint32_t lost = disconnects[0];
    peer = Peer::Healthy;
    run(client, 6000);
    assert(attempts.size() == 2 && attempts[1]-lost == 5000);
  }
  reset(Peer::HeaderTimeout);
  {
    Client client;
    begin(client);
    run(client, 20000);
    assert(attempts.size() == 2 && gap(1) >= 10000 && gap(1) <= 10020);
  }
  reset(Peer::Healthy, 0);
  {
    Client client;
    begin(client, 0);
    run(client, 100);
    assert(attempts.size() == 1 && attempts[0] == 0 && client.connected());
  }
  reset(Peer::Refuse);
  {
    Client client;
    begin(client);
    run(client, 20000);
    assert(attempts.size() == 2);
    peer = Peer::Healthy;
    run(client, 60000);
    assert(attempts.size() == 3 && client.connected());
    uint32_t dropped = now;
    client.dropTcp();
    run(client, 10000);
    assert(attempts.size() == 4 && attempts.back()-dropped == 5000);
  }
  reset(Peer::Refuse, 60000, 12000, 1);
  {
    Client client;
    begin(client);
    run(client, 100000);
    assert(attempts.size() >= 3 && gap(1) >= 17000 && gap(1) <= 17020 &&
           gap(2) >= 60000 && gap(2) <= 60020);
  }
  std::puts("PASS websocket reconnect pacing, lifecycle and 32-bit rollover");
}
