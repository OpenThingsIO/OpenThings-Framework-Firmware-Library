#pragma once

#include <stdint.h>

// One retry clock per client. No sleeps, allocations, or transport callbacks.
class ReconnectPolicy {
public:
  void configure(uint32_t now, uint32_t seed, uint32_t interval) {
    rng = seed ? seed : 0x91e10da5u;
    base = interval < 2000 ? 2000 : (interval > 300000 ? 300000 : interval);
    next = base;
    connected = false;
    activity = false;
    since = now;
    delay = 1000 + random() % 4001;
  }
  bool ready(uint32_t now) const { return uint32_t(now - since) >= delay; }
  void rejected() { next = 300000; }
  void failed(uint32_t now) {
    connected = false;
    activity = false;
    since = now;
    delay = next / 2 + random() % (next - next / 2 + 1);
    next = next >= 150000 ? 300000 : next * 2;
  }
  void opened(uint32_t now) {
    connected = true;
    activity = false;
    openedAt = now;
  }
  void received(uint32_t now) { activity = true; lastActivity = now; }
  void update(uint32_t now) {
    if (connected && activity && uint32_t(now - openedAt) >= 60000 &&
        uint32_t(now - lastActivity) <= 30000) next = base;
  }
private:
  uint32_t random() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
  uint32_t rng = 1, base = 5000, next = 5000;
  uint32_t since = 0, delay = 0, openedAt = 0, lastActivity = 0;
  bool connected = false, activity = false;
};
