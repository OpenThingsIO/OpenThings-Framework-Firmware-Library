#include "Websocket.h"
#include <cassert>
#include <cstdio>
#include <set>

namespace ReconnectTest {
uint32_t now, connectDuration, cleanupDuration;
Peer peer;
std::vector<uint32_t> attempts, disconnects;
}
uint32_t millis() { return ReconnectTest::now; }
uint32_t esp_random() { return 0x374b08a5; }
long secureRandom(long maximum) { return esp_random() % maximum; }
FakeESP ESP;

class Client : public WebsocketClient {
public:
  void tick() { advancePeer(); poll(); }
  void dropTcp() { tcpUp = false; }
  void pong() { if (callback) callback(WStype_PONG, nullptr, 0); }
  void rejectAuth() {
    uint8_t reason[] = "HTTP 401";
    if (callback) callback(WStype_DISCONNECTED, reason, 8);
    dropTcp();
  }
  bool connected() const { return _client.status == WSC_CONNECTED; }
};
static void reset(ReconnectTest::Peer peer, uint32_t start = 60000,
                  uint32_t duration = 0, uint32_t cleanup = 0) {
  ReconnectTest::now = start;
  ReconnectTest::peer = peer;
  ReconnectTest::connectDuration = duration;
  ReconnectTest::cleanupDuration = cleanup;
  ReconnectTest::attempts.clear();
  ReconnectTest::disconnects.clear();
}
static void begin(Client &client) {
  client.connect("test.invalid", 80, "/socket");
  client.setReconnectInterval(5000);
}
static void run(Client &client, uint32_t duration, bool traffic = false) {
  uint32_t start = millis();
  while (uint32_t(millis()-start) < duration) {
    client.tick();
    if (traffic && client.connected()) client.pong();
    ReconnectTest::now += 10;
  }
}
static void outage(ReconnectTest::Peer peer, uint32_t start = 60000,
                   uint32_t duration = 0, uint32_t cleanup = 0) {
  using namespace ReconnectTest;
  reset(peer, start, duration, cleanup);
  Client client;
  begin(client);
  client.enableHeartbeat(100, 50, 1);
  run(client, 900000);
  assert(attempts.size() >= 8 && attempts.size() <= 13);
  assert(uint32_t(attempts[0]-start) >= 1000 && uint32_t(attempts[0]-start) <= 5010);
  uint32_t base = 5000;
  for (size_t i=1; i<attempts.size(); ++i) {
    uint32_t gap = attempts[i]-attempts[i-1];
    assert(gap >= base/2+duration);
    assert(gap <= base+duration+cleanup+5300);
    base = base >= 150000 ? 300000 : base*2;
  }
}
int main() {
  using namespace ReconnectTest;
  // Identical random output (for example RF-off) still mixes in device identity.
  std::set<uint32_t> deviceStartup;
  for (uint32_t id = 1; id <= 100; ++id) {
    reset(Peer::Refuse);
    ESP.chipId = id;
    ESP.mac = 0xaabbcc000000ULL | id;
    Client client;
    begin(client);
    run(client, 5100);
    assert(!attempts.empty());
    deviceStartup.insert(attempts.front());
  }
  assert(deviceStartup.size() > 50);
  for (Peer p : {Peer::Refuse, Peer::SilentBeforeUpgrade, Peer::SilentAfterUpgrade,
                 Peer::HttpReject, Peer::CloseFrame, Peer::HeaderTimeout,
                 Peer::HeartbeatTimeout}) {
    outage(p);
    outage(p, 0xfffff000u, 30, 1);
  }
  outage(Peer::Refuse, 2000, 12000, 1);
  outage(Peer::Refuse, 0xfffff000u, 12000, 1);

  // A handshake alone must not reset the high backoff; sustained traffic must.
  for (bool stable : {false, true}) {
    reset(Peer::Refuse);
    Client client;
    begin(client);
    run(client, 600000);
    peer = Peer::Healthy;
    run(client, 370000, stable);
    assert(client.connected());
    size_t count = attempts.size();
    uint32_t drop = now;
    client.dropTcp();
    run(client, stable ? 6000 : 310000);
    assert(attempts.size() == count+1 && client.connected());
    uint32_t gap = attempts.back()-drop;
    assert(stable ? (gap >= 2500 && gap <= 5020) : (gap >= 150000 && gap <= 300020));
  }
  reset(Peer::Healthy);
  {
    Client client;
    begin(client);
    run(client, 6000);
    assert(client.connected());
    client.close(false);
    size_t n = attempts.size();
    run(client, 600000);
    assert(attempts.size() == n);
    begin(client);
    run(client, 6000);
    assert(attempts.size() == n+1 && client.connected());
    client.onEvent([&](WSEvent_t, uint8_t *, size_t) { client.poll(); });
    client.close(true);
    run(client, 6000);
    assert(attempts.size() == n+2);
  }
  // Independently seeded fleet: startup and capped retries must disperse.
  reset(Peer::Healthy);
  {
    Client client;
    begin(client);
    run(client, 6000);
    client.rejectAuth();
    size_t n = attempts.size();
    run(client, 149000);
    assert(attempts.size() == n);
    run(client, 152000);
    assert(attempts.size() == n+1);
  }
  std::set<uint32_t> startup, capped;
  for (uint32_t seed=1; seed<=2000; ++seed) {
    ReconnectPolicy p;
    p.configure(0xfffff000u, seed, 5000);
    uint32_t elapsed=0;
    while (!p.ready(0xfffff000u+elapsed)) ++elapsed;
    assert(elapsed >= 1000 && elapsed <= 5000);
    startup.insert(elapsed);
    for (int i=0;i<8;++i) p.failed(0xfffff000u);
    elapsed=0;
    while (!p.ready(0xfffff000u+elapsed)) elapsed+=100;
    assert(elapsed >= 150000 && elapsed <= 300100);
    capped.insert(elapsed);
  }
  assert(startup.size()>1000 && capped.size()>800);
  std::puts("PASS exponential jitter, failure paths, stable reset, reentrancy, fleet and rollover");
}
