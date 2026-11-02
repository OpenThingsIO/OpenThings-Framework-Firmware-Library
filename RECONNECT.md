# Reconnect scheduling

The synchronous Arduino and Linux backends share `ReconnectPolicy`.

- Startup/configuration delay: uniformly selected integer milliseconds in 1-5 s.
- Default failure bases: 5, 10, 20, 40, 80, 160, then 300 s capped.
- Each failure schedules a delay in 50-100% of its base, measured after the
  failed attempt/socket cleanup completes, not from the start of a blocking dial.
- Jitter uses a private per-client PRNG seeded by ESP8266 hardware random,
  ESP32 `esp_random()`, or Linux `getrandom()`. Arduino seeds also mix in the
  chip ID/eFuse MAC for Ethernet units whose RF is off. Linux entropy failure falls back
  to monotonic nanoseconds/seconds and PID. No shared global PRNG is reseeded.
- Reset requires at least 60 s continuously connected and a received ping,
  pong, text, or binary frame within the preceding 30 s. Handshake alone is
  insufficient. A silent or short-lived upgraded connection retains its backoff.
- Explicit Arduino HTTP 401/403 rejection reasons immediately select the capped
  150-300 s cooldown. Generic disconnects and HTTP 404 do not mean invalid auth.
- `close(false)` disables reconnect on both backends; `close(true)` preserves
  escalation. Deliberate `connect()` or `connectSecure()` reconfiguration closes
  the prior transport and initializes a fresh startup delay.
- Network restoration does not reset the retry clock. OpenSprinkler retains
  the OTF instance across ordinary link/Wi-Fi changes.
- A reentrancy guard permits one `poll()` at a time. Arduino's internal dial
  path is gated except for an explicitly authorized attempt; this also handles
  a socket dying between the wrapper's check and the library's `loop()`.
- Unsigned 32-bit elapsed arithmetic handles millis rollover. Linux uses
  CLOCK_MONOTONIC, not gettimeofday, for heartbeat/retry scheduling.

The five-minute cap preserves the previous refused-connection maximum while
extending escalation to short-lived connections. It trades slower recovery
(up to five minutes plus transport time/poll scheduling) for reduced outage load.
`setReconnectInterval()` configures the first base, clamped to 2-300 s; it is
an explicit policy reset, not a function to call on every network notification.

## Scope limits

The retry **waiting** is nonblocking. The existing DNS, TCP/TLS connect, send,
and handshake implementations are synchronous. This patch does not turn them
into asynchronous transports or establish a new worst-case watering-loop bound.
Transport timeout behavior, including slow DNS and incomplete upgrade headers,
still needs hardware/backend-specific validation. Do not advertise this as a
fully nonblocking networking implementation.

The companion TinyWebsockets Linux fix exits on EOF/read failure, including
mid-header disconnects, and gives outgoing sockets 10-second receive/send
timeouts (the send timeout also bounds Linux TCP connect). These are per-call
timeouts: DNS remains resolver-bounded, and a slowly trickling response is not
an overall-handshake deadline. A synchronous attempt can still delay watering.

Retry-After is not exposed by the current wrapper interfaces and is not parsed.
Linux TinyWebsockets does not expose handshake HTTP status to this wrapper, so
its explicit-auth failures use the normal exponential path. Replacing or
extending handshake parsing is separate work. The built-in OTF `useSsl` branch
currently does not initiate a connection; this existing limitation is unchanged.
Post-upgrade `ERR:`/close rejections also use normal escalation, not the
explicit HTTP 401/403 shortcut.

An outage affecting every controller still produces a first retry wave within
2.5-5 seconds. Server-side admission/rate limits are needed for that wave and
for older firmware that does not contain this policy.

No program saves or other cloud messages are replayed. Deferred-request lifetime
and connection-generation checks remain unchanged. Existing local/cloud fairness,
Linux interrupted-stream state, and TCP TIME_WAIT issues are not addressed here.

## Validation

`make -C test test` covers the Arduino wrapper on ESP8266/ESP32-shaped fakes,
including refused connects, silent pre/post-upgrade drops, HTTP rejection, close
frames, header/heartbeat timeouts, slow calls, stale socket cleanup, rollover,
stable-session reset, disable/reconfigure, reentrant callbacks, auth cooldown,
and dispersion across 2,000 independently seeded policies. These are lifecycle
models, not live DNS/TLS or hardware timing measurements.

`test/test_reconnect_loopback.py` runs a real Linux wrapper against a local-only
WebSocket fixture, repeatedly upgrades then drops, holds a healthy connection
for 65 s, drops again, and checks the short retry delay after healthy reset.
With TinyWebsockets in the sibling directory, run
`make -C test test-reconnect-loopback` (about 150 seconds).

Physical W5500/CH390/Wi-Fi outage, memory, and watering validation remains required
before release. Do not flash or repoint an ongoing soak just to run these tests.
