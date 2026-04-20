# 📋 OpenThings Framework Multi-Client - Quick Reference

## 🚀 Quick Start (30 Sekunden)

```cpp
#include "OpenThingsFramework.h"

// 1. Erstelle Server
OTF::Esp32LocalServer server(80, 443);  // HTTP + HTTPS

// 2. Starte Server
void setup() {
  server.begin();
}

// 3. Akzeptiere Clients (Alte API)
void loop() {
  OTF::LocalClient *client = server.acceptClient();
  if (client) {
    // Process...
  }
}
```

---

## 📚 Essential APIs

### Server-Erstellung
```cpp
// Standard (4 Clients max)
OTF::Esp32LocalServer server(80, 443);

// Custom (8 Clients max)
OTF::Esp32LocalServer server(80, 443, 8);

// HTTP-only (kein HTTPS)
OTF::Esp32LocalServer server(80, 0);
```

### Client-Verwaltung
```cpp
// Accept new client (non-blocking)
OTF::LocalClient *newClient = server.acceptClientNonBlocking();

// Anzahl aktive Clients
uint16_t count = server.getActiveClientCount();

// Zugriff auf Client im Pool
OTF::LocalClient *client = server.getClientAtIndex(0);

// Schließe alle Clients
server.closeAllClients();

// Check request type (HTTP vs HTTPS)
bool isHttps = server.isCurrentRequestHttps();
```

### Client-Operationen
```cpp
// Daten lesen
char buffer[512];
size_t bytes = client->readBytes(buffer, 512);
size_t bytes = client->readBytesUntil('\n', buffer, 512);

// Daten schreiben
client->print("Hello");
client->write((const char*)data, length);
client->flush();

// Steuerung
client->dataAvailable();
client->setTimeout(5000);
client->stop();
```

---

## ⚙️ Konfiguration (platformio.ini)

```ini
build_flags =
  -DOTF_MAX_CONCURRENT_CLIENTS=4
  -DOTF_USE_PSRAM=1
  -DOTF_CLIENT_READ_BUFFER_SIZE=4096
  -DOTF_ENABLE_WRITE_BUFFERING=1
  -DOTF_ENABLE_TCP_NODELAY=1
```

### Default-Werte pro Platform
| Setting | ESP32-C5 | ESP32 | ESP32-S3 |
|---------|----------|-------|----------|
| Max Clients | 3 | 4 | 8 |
| PSRAM | ❌ | ✅ | ✅ |
| Read Buffer | 2KB | 4KB | 4KB |
| Write Buffer | 4KB | 8KB | 8KB |

---

## 🔍 Debugging

### Serial-Output aktivieren
```cpp
#define ENABLE_DEBUG
#define OTF_DEBUG_MEMORY 1
#define OTF_DEBUG_CLIENT_LIFECYCLE 1
```

### Memory-Check
```cpp
Serial.printf("DRAM: %d, PSRAM: %d\n", 
  ESP.getFreeHeap(), ESP.getFreePsram());
```

### Performance-Monitoring
```cpp
#include "Esp32Performance.h"

OTF::PerformanceMonitor monitor;

monitor.recordConnection();
monitor.recordResponseTime(millis() - start);
monitor.printMetrics(server.getActiveClientCount());
monitor.printOptimizationRecommendations(server.getActiveClientCount());
```

---

## 💡 Common Patterns

### Pattern 1: Single-Client Loop (Kompatibilität)
```cpp
void loop() {
  OTF::LocalClient *client = server.acceptClient();
  if (client && client->dataAvailable()) {
    char buf[256];
    size_t len = client->readBytes(buf, 256);
    // Process...
    client->stop();
  }
}
```

### Pattern 2: Multi-Client Loop (Neu)
```cpp
std::vector<OTF::LocalClient*> clients;

void loop() {
  // Accept new
  OTF::LocalClient *newClient = server.acceptClientNonBlocking();
  if (newClient) clients.push_back(newClient);
  
  // Process all
  for (auto &c : clients) {
    if (c && c->dataAvailable()) {
      // Process...
    }
  }
}
```

### Pattern 3: Request Processing
```cpp
void handleClient(OTF::LocalClient *client) {
  char buf[512];
  
  // Read request line
  client->readBytesUntil('\n', buf, 512);
  
  // Send response
  client->print("HTTP/1.1 200 OK\r\n");
  client->print("Content-Type: text/plain\r\n");
  client->print("Content-Length: 5\r\n\r\n");
  client->print("Hello");
  client->flush();
  
  client->stop();
}
```

### Pattern 4: Speicher-Management
```cpp
void cleanupClients(std::vector<OTF::LocalClient*> &clients) {
  for (auto it = clients.begin(); it != clients.end(); ) {
    OTF::LocalClient *c = *it;
    if (!c || !c->dataAvailable()) {
      if (c) delete c;
      it = clients.erase(it);
    } else {
      ++it;
    }
  }
}
```

---

## 📊 Performance Tips

### Speicher optimieren
```cpp
// PSRAM priorisieren
#define OTF_USE_PSRAM 1

// Buffer-Größen anpassen (kleinere = weniger Memory)
#define OTF_CLIENT_READ_BUFFER_SIZE 2048

// Connection Pool reduzieren bei Speichermangel
#define OTF_MAX_CONCURRENT_CLIENTS 2
```

### Response-Zeit optimieren
```cpp
// TCP_NODELAY aktivieren (Standard)
#define OTF_ENABLE_TCP_NODELAY 1

// Write Buffering aktivieren
#define OTF_ENABLE_WRITE_BUFFERING 1

// TLS Record Size limitieren (für C5)
#define OTF_TLS_MAX_RECORD_SIZE 2048
```

### Durchsatz optimieren
```cpp
// Mehr Clients erlauben
#define OTF_MAX_CONCURRENT_CLIENTS 6

// Größere Buffer
#define OTF_CLIENT_WRITE_BUFFER_SIZE 16384

// Keep-Alive aktivieren
#define OTF_ENABLE_KEEP_ALIVE 1
```

---

## ⚠️ Häufige Probleme

| Problem | Ursache | Lösung |
|---------|---------|--------|
| "Max clients reached" | Zu viele Connections | Erhöhe MAX_CONCURRENT_CLIENTS oder schließe alte Clients |
| Memory crash | PSRAM allokation fehlgeschlagen | Reduziere Buffer-Größen oder nutze nur DRAM |
| Slow responses | Nagle's Algorithm aktiv | Nutze TCP_NODELAY |
| TLS Handshake Fehler | Zu wenig Memory | Reduziere aktive Clients oder Buffer-Größen |
| PSRAM nicht genutzt | psramFound() false | Überprüfe Hardware, setze OTF_USE_PSRAM=0 |

---

## 🔧 Troubleshooting

### Step 1: Check Memory
```cpp
Serial.printf("DRAM: %d, PSRAM: %d\n", ESP.getFreeHeap(), ESP.getFreePsram());
Serial.printf("PSRAM available: %s\n", psramFound() ? "YES" : "NO");
```

### Step 2: Enable Debug
```cpp
#define ENABLE_DEBUG 1
#define OTF_DEBUG_MEMORY 1
#define OTF_DEBUG_CLIENT_LIFECYCLE 1
```

### Step 3: Monitor Performance
```cpp
OTF::PerformanceMonitor monitor;
// ... recordieren
monitor.printOptimizationRecommendations(server.getActiveClientCount());
```

### Step 4: Adjust Config
```cpp
// Für ESP32-C5 (Low-Memory):
#define OTF_MAX_CONCURRENT_CLIENTS 2
#define OTF_CLIENT_READ_BUFFER_SIZE 1024
#define OTF_USE_PSRAM 0
```

---

## 📖 Weitere Informationen

- **Detailliertes Handbuch**: [MULTICLIENT_GUIDE.md](./MULTICLIENT_GUIDE.md)
- **Technische Details**: [TECHNICAL_OVERVIEW.md](./TECHNICAL_OVERVIEW.md)
- **Beispiel-Code**: [example_multiclient_server.ino](./example_multiclient_server.ino)
- **Profiler Tool**: [profile_performance.ino](./profile_performance.ino)
- **Konfiguration**: [Esp32LocalServer_Config.h](./Esp32LocalServer_Config.h)
- **Performance Monitor**: [Esp32Performance.h](./Esp32Performance.h)

---

## ✅ Checklist: Integration

- [ ] Neue Header-Dateien kopiert
- [ ] Esp32LocalServer.cpp mit neuer Version ersetzt
- [ ] Config-Werte in platformio.ini angepasst
- [ ] Code mit ENABLE_DEBUG getestet
- [ ] Performance mit profile_performance.ino gemessen
- [ ] Beispiel-Sketch (example_multiclient_server.ino) lädt
- [ ] Memory-Metriken überprüft
- [ ] Produktions-Code released

---

## 🎯 Ziele erreicht

✅ Multiple Verbindungen (4-8 concurrent)
✅ PSRAM-Nutzung (automatische Erkennung)
✅ Optimierte Zugriffszeit (TCP_NODELAY + Buffering)
✅ Rückwärts-kompatibel (alte API funktioniert)
✅ Dokumentiert (umfangreich)
✅ Beispiele (produktiv ready)
✅ Performance-Tools (Profiler + Monitor)
