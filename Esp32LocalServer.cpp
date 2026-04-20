#if defined(ESP32)
#ifdef __has_include
  #if __has_include("sdkconfig.h")
    #include "sdkconfig.h"  // ESP-IDF configuration - must be first for MBEDTLS defines
  #endif
#endif
#include "Esp32LocalServer.h"
#include "Esp32LocalServer_Config.h"
#include <mbedtls/error.h>  // For mbedtls_strerror

#ifndef OTF_DEBUG
  #if defined(SERIAL_DEBUG) || defined(OTF_DEBUG_MODE)
    #define OTF_DEBUG(...) Serial.printf(__VA_ARGS__)
  #else
    #define OTF_DEBUG(...)
  #endif
#endif

// Custom certificate override pointers are defined in custom_cert.cpp
// and declared extern in Esp32LocalServer.h

// Use namespace OTF for all class implementations
using namespace OTF;

static int wifi_client_send(void *ctx, const unsigned char *buf, size_t len) {
  WiFiClient *client = static_cast<WiFiClient*>(ctx);
  if (!client || !client->connected()) return MBEDTLS_ERR_NET_CONN_RESET;
  int written = client->write(buf, len);
  if (written == 0) return MBEDTLS_ERR_SSL_WANT_WRITE;
  if (written < 0) return MBEDTLS_ERR_NET_SEND_FAILED;
  return written;
}

static int wifi_client_recv(void *ctx, unsigned char *buf, size_t len) {
  WiFiClient *client = static_cast<WiFiClient*>(ctx);
  if (!client || !client->connected()) return MBEDTLS_ERR_NET_CONN_RESET;
  int available = client->available();
  if (available == 0) return MBEDTLS_ERR_SSL_WANT_READ;
  int read = client->read(buf, len);
  if (read < 0) return MBEDTLS_ERR_NET_RECV_FAILED;
  return read;
}

// ============================================================================
// WiFiSecureServer Implementation (SSL/TLS with mbedTLS)
// ============================================================================

WiFiSecureServer::WiFiSecureServer(uint16_t port, const unsigned char* cert, uint16_t certLen, 
                                   const unsigned char* key, uint16_t keyLen)
  : server(port, 5), port(port),
    certData(cert), certLength(certLen),
    keyData(key), keyLength(keyLen), initialized(false) {
  
  // Initialize mbedTLS structures
  mbedtls_ssl_config_init(&sslConf);
  mbedtls_x509_crt_init(&serverCert);
  mbedtls_pk_init(&serverKey);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctrDrbg);
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
  mbedtls_ssl_ticket_init(&ticketCtx);
  ticketCtxInitialized = false;
#endif
  // Initialize SSL context pool
  for (int i = 0; i < SSL_CTX_POOL_SIZE; i++) {
    sslPool[i] = nullptr;
    sslPoolInUse[i] = false;
  }
}

WiFiSecureServer::~WiFiSecureServer() {
  // Free SSL context pool
  for (int i = 0; i < SSL_CTX_POOL_SIZE; i++) {
    if (sslPool[i]) {
      mbedtls_ssl_free(sslPool[i]);
      delete sslPool[i];
      sslPool[i] = nullptr;
    }
  }
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
  if (ticketCtxInitialized) {
    mbedtls_ssl_ticket_free(&ticketCtx);
  }
#endif
  mbedtls_ssl_config_free(&sslConf);
  mbedtls_x509_crt_free(&serverCert);
  mbedtls_pk_free(&serverKey);
  mbedtls_entropy_free(&entropy);
  mbedtls_ctr_drbg_free(&ctrDrbg);
}

bool WiFiSecureServer::setupSSLContext() {
  int ret;
  
  // Seed the random number generator
  const char* pers = "esp32_https_server";
  ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
                               (const unsigned char*)pers, strlen(pers));
  if (ret != 0) {
    OTF_DEBUG("mbedtls_ctr_drbg_seed failed: -0x%x\n", -ret);
    return false;
  }
  
  // Setup SSL configuration defaults for server
  ret = mbedtls_ssl_config_defaults(&sslConf,
                                    MBEDTLS_SSL_IS_SERVER,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    OTF_DEBUG("mbedtls_ssl_config_defaults failed: -0x%x\n", -ret);
    return false;
  }

  // TLS version: Let mbedtls_ssl_config_defaults set it based on SDK compile flags.
  // Overriding explicitly can cause MBEDTLS_ERR_SSL_BAD_CONFIG (-0x5e80).
#if defined(CONFIG_MBEDTLS_SSL_PROTO_TLS1_3) && CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
  OTF_DEBUG(">>> TLS 1.3 enabled in SDK <<<\n");
#elif defined(CONFIG_MBEDTLS_SSL_PROTO_TLS1_2) && CONFIG_MBEDTLS_SSL_PROTO_TLS1_2
  OTF_DEBUG(">>> TLS 1.2 only (TLS 1.3 not available) <<<\n");
#endif
  
  // Set random number generator
  mbedtls_ssl_conf_rng(&sslConf, mbedtls_ctr_drbg_random, &ctrDrbg);
  
  // No client certificate verification needed (self-signed server cert)
  mbedtls_ssl_conf_authmode(&sslConf, MBEDTLS_SSL_VERIFY_NONE);
  
  // ==========================================================================
  // Supported groups/curves — ordered by performance for TLS 1.3 key exchange.
  // x25519 is fastest (~25% faster than P-256 ECDHE) and preferred by all
  // modern TLS 1.3 clients. Without x25519, clients send a P-256 key share
  // or trigger HelloRetryRequest (extra round trip = ~100ms penalty).
  // P-256 is needed because our server certificate uses a P-256 ECDSA key.
  // ==========================================================================
  static const uint16_t supported_groups[] = {
    MBEDTLS_SSL_IANA_TLS_GROUP_X25519,     // Fastest for TLS 1.3 key exchange
    MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1,  // P-256 — HW accel, matches our cert
    MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1,  // P-384 — HW accel, fallback
    MBEDTLS_SSL_IANA_TLS_GROUP_NONE
  };
  mbedtls_ssl_conf_groups(&sslConf, supported_groups);
  OTF_DEBUG("Configured groups: x25519, secp256r1, secp384r1\n");
  
  // ==========================================================================
  // Session Tickets — CRITICAL for HTTPS performance!
  // Without tickets, every connection requires a full TLS handshake:
  //   TLS 1.3: 1-RTT + ECDHE + ECDSA-verify (~150-300ms on ESP32-C5)
  //   TLS 1.2: 2-RTT + ECDHE + ECDSA-verify (~250-500ms on ESP32-C5)
  // With tickets, resumption is 0-RTT (TLS 1.3) or 1-RTT (TLS 1.2):
  //   ~20-50ms — just symmetric crypto, no expensive ECC operations.
  // ==========================================================================
#if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_SRV_C)
  ret = mbedtls_ssl_ticket_setup(&ticketCtx,
                                  mbedtls_ctr_drbg_random, &ctrDrbg,
                                  MBEDTLS_CIPHER_AES_256_GCM,
                                  86400);  // 24h ticket lifetime
  if (ret == 0) {
    ticketCtxInitialized = true;
    mbedtls_ssl_conf_session_tickets_cb(&sslConf,
                                         mbedtls_ssl_ticket_write,
                                         mbedtls_ssl_ticket_parse,
                                         &ticketCtx);
    mbedtls_ssl_conf_session_tickets(&sslConf, MBEDTLS_SSL_SESSION_TICKETS_ENABLED);
    OTF_DEBUG("Session tickets ENABLED (AES-256-GCM, 24h lifetime)\n");
  } else {
    OTF_DEBUG("Session ticket setup failed: -0x%x (tickets disabled)\n", -ret);
    mbedtls_ssl_conf_session_tickets(&sslConf, MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
  }
#endif
  
  // Read timeout for faster error detection (2s; default 60s wastes time on dead connections)
  mbedtls_ssl_conf_read_timeout(&sslConf, 2000);
  
  // Extended Master Secret: ENABLED for TLS 1.2 compatibility.
  // Some clients (Android, iOS) require EMS; disabling causes handshake failures.
  #if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET)
    mbedtls_ssl_conf_extended_master_secret(&sslConf, MBEDTLS_SSL_EXTENDED_MS_ENABLED);
  #endif
  
  // Encrypt-then-MAC: minor overhead on TLS 1.2 CBC suites; not used with GCM/TLS 1.3
  #if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
    mbedtls_ssl_conf_encrypt_then_mac(&sslConf, MBEDTLS_SSL_ETM_DISABLED);
  #endif

  // Disable renegotiation (TLS 1.2 only; deprecated, security risk)
  #if defined(MBEDTLS_SSL_RENEGOTIATION)
    mbedtls_ssl_conf_renegotiation(&sslConf, MBEDTLS_SSL_RENEGOTIATION_DISABLED);
  #endif
  
  // Don't send CA list in CertificateRequest (we don't request client certs)
  #if defined(MBEDTLS_SSL_CERT_REQ_CA_LIST)
    mbedtls_ssl_conf_cert_req_ca_list(&sslConf, MBEDTLS_SSL_CERT_REQ_CA_LIST_DISABLED);
  #endif
  
  return true;
}

bool WiFiSecureServer::setupCertificate() {
  int ret;
  
  OTF_DEBUG("Loading certificate: %d bytes\n", certLength);
  OTF_DEBUG("Loading private key: %d bytes\n", keyLength);
  OTF_DEBUG("setupCertificate: before parse: heap=%d bytes, largest block=%d bytes\n",
            ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  
  // Certificate and key are in PROGMEM (Flash) to save RAM
  // mbedTLS can read directly from Flash on ESP32
  // Parse certificate (DER format)
  ret = mbedtls_x509_crt_parse_der(&serverCert, certData, certLength);
  if (ret != 0) {
    OTF_DEBUG("mbedtls_x509_crt_parse_der failed: -0x%x\n", -ret);
    return false;
  }
  OTF_DEBUG("Certificate parsed successfully\n");
  OTF_DEBUG("setupCertificate: after cert: heap=%d bytes, largest block=%d bytes\n",
            ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  
  // Parse private key (DER format)
  // The key in cert.h is in SEC1 format with ECParameters
  // First try standard parsing with NULL password
  ret = mbedtls_pk_parse_key(&serverKey, keyData, keyLength, NULL, 0, mbedtls_ctr_drbg_random, &ctrDrbg);
  
  if (ret != 0) {
    OTF_DEBUG("mbedtls_pk_parse_key failed: -0x%x\n", -ret);
    return false;
  }
  OTF_DEBUG("Private key parsed successfully\n");
  OTF_DEBUG("setupCertificate: after key: heap=%d bytes, largest block=%d bytes\n",
            ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  
  // Set certificate and key in SSL config
  ret = mbedtls_ssl_conf_own_cert(&sslConf, &serverCert, &serverKey);
  if (ret != 0) {
    OTF_DEBUG("mbedtls_ssl_conf_own_cert failed: -0x%x\n", -ret);
    return false;
  }
  
  OTF_DEBUG("SSL certificate and private key loaded\n");
  return true;
}

bool WiFiSecureServer::begin() {
  OTF_DEBUG("WiFiSecureServer::begin() starting...\n");
  
  // Setup SSL context
  if (!setupSSLContext()) {
    OTF_DEBUG("setupSSLContext() failed!\n");
    return false;
  }
  
  // Load certificate and key
  if (!setupCertificate()) {
    OTF_DEBUG("setupCertificate() failed!\n");
    return false;
  }
  
  // Start underlying WiFi server
  server.begin();
  initialized = true;
  OTF_DEBUG("WiFiSecureServer started on port %d\n", port);
  
  return true;
}

WiFiClient WiFiSecureServer::accept() {
  return server.accept();
}

void WiFiSecureServer::end() {
  server.end();
  OTF_DEBUG("WiFiSecureServer on port %d stopped\n", port);
}
// Note: Direct buffering removed - simplicity preferred for embedded systems
// Clients are managed directly by Esp32LocalServer

mbedtls_ssl_context* WiFiSecureServer::handshakeSSL(WiFiClient* wifiClient) {
  if (!initialized || !wifiClient || !wifiClient->connected()) {
    OTF_DEBUG("handshakeSSL: bad state (init=%d, client=%p)\n", initialized, (void*)wifiClient);
    if (wifiClient) wifiClient->stop();
    return NULL;
  }

  // Fast-path: reject non-TLS traffic (first byte != 0x16 = TLS handshake)
  int avail = wifiClient->available();
  if (avail > 0) {
    int first = wifiClient->peek();
    if (first >= 0 && first != 0x16) {
      OTF_DEBUG("Non-TLS on HTTPS port (0x%02x). Closing.\n", first);
      wifiClient->stop();
      return NULL;
    }
  }
  
  // CRITICAL: Set TCP_NODELAY BEFORE handshake to prevent Nagle's algorithm
  // from buffering small TLS handshake packets (ClientHello is ~300 bytes).
  // Without this, each handshake message can be delayed up to 200ms.
  wifiClient->setNoDelay(true);
  
  OTF_DEBUG("SSL handshake starting (heap=%d, max_block=%d)\n", 
            ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  
  unsigned long handshakeStart = millis();
  const unsigned long HANDSHAKE_TIMEOUT = OTF_SSL_HANDSHAKE_TIMEOUT_MS;
  
  // Try to get an SSL context from the pool (avoids expensive alloc/init)
  mbedtls_ssl_context* ssl = nullptr;
  int poolSlot = -1;
  for (int i = 0; i < SSL_CTX_POOL_SIZE; i++) {
    if (sslPool[i] && !sslPoolInUse[i]) {
      // Reuse pooled context via session_reset (preserves session cache)
      int ret = mbedtls_ssl_session_reset(sslPool[i]);
      if (ret == 0) {
        ssl = sslPool[i];
        sslPoolInUse[i] = true;
        poolSlot = i;
        OTF_DEBUG("SSL ctx reused from pool[%d]\n", i);
        break;
      }
      // Reset failed — free and recreate
      mbedtls_ssl_free(sslPool[i]);
      delete sslPool[i];
      sslPool[i] = nullptr;
    }
  }
  
  if (!ssl) {
    // No pooled context available — allocate new one
    ssl = new (std::nothrow) mbedtls_ssl_context;
    if (!ssl) {
      OTF_DEBUG("SSL ctx alloc failed!\n");
      wifiClient->stop();
      return NULL;
    }
    mbedtls_ssl_init(ssl);
    
    int ret = mbedtls_ssl_setup(ssl, &sslConf);
    if (ret != 0) {
      char errBuf[80];
      mbedtls_strerror(ret, errBuf, sizeof(errBuf));
      OTF_DEBUG("ssl_setup failed: -0x%x (%s)\n", -ret, errBuf);
      mbedtls_ssl_free(ssl);
      delete ssl;
      wifiClient->stop();
      return NULL;
    }
    
    // Find a pool slot for this new context
    for (int i = 0; i < SSL_CTX_POOL_SIZE; i++) {
      if (!sslPool[i]) {
        sslPool[i] = ssl;
        sslPoolInUse[i] = true;
        poolSlot = i;
        OTF_DEBUG("SSL ctx stored in pool[%d]\n", i);
        break;
      }
    }
  }

  mbedtls_ssl_set_bio(ssl, wifiClient, wifi_client_send, wifi_client_recv, NULL);

  // Handshake loop — use yield() + 1ms delay to minimize latency while
  // allowing FreeRTOS scheduler to run WiFi/system tasks.
  int ret;
  while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      if (ret == MBEDTLS_ERR_NET_CONN_RESET) {
        OTF_DEBUG("Handshake: connection reset\n");
      } else {
        char errBuf[80];
        mbedtls_strerror(ret, errBuf, sizeof(errBuf));
        OTF_DEBUG("Handshake failed: -0x%x (%s)\n", -ret, errBuf);
      }
      // Return context to pool instead of deleting
      if (poolSlot >= 0) {
        sslPoolInUse[poolSlot] = false;
      } else {
        mbedtls_ssl_free(ssl);
        delete ssl;
      }
      wifiClient->stop();
      return NULL;
    }
    
    if (millis() - handshakeStart > HANDSHAKE_TIMEOUT) {
      OTF_DEBUG("Handshake timeout (%ums)!\n", (unsigned)HANDSHAKE_TIMEOUT);
      if (poolSlot >= 0) {
        sslPoolInUse[poolSlot] = false;
      } else {
        mbedtls_ssl_free(ssl);
        delete ssl;
      }
      wifiClient->stop();
      return NULL;
    }
    
    delay(1);  // 1ms yield (was 10ms — saved ~90ms per handshake)
  }
  
  unsigned long elapsed = millis() - handshakeStart;
  OTF_DEBUG("TLS handshake OK in %ums: %s, %s\n",
            (unsigned)elapsed,
            mbedtls_ssl_get_version(ssl),
            mbedtls_ssl_get_ciphersuite(ssl));

  return ssl;
}

// Return an SSL context to the pool for reuse
void WiFiSecureServer::returnSSLContext(mbedtls_ssl_context* ssl) {
  if (!ssl) return;
  for (int i = 0; i < SSL_CTX_POOL_SIZE; i++) {
    if (sslPool[i] == ssl) {
      sslPoolInUse[i] = false;
      OTF_DEBUG("SSL ctx returned to pool[%d]\n", i);
      return;
    }
  }
  // Not from pool — just free it
  mbedtls_ssl_free(ssl);
  delete ssl;
}

// ============================================================================
// Esp32LocalServer — Single-Client Stateless Implementation
// ============================================================================

Esp32LocalServer::Esp32LocalServer(uint16_t port, uint16_t httpsPort) 
  : httpServer(port, 5),    // backlog=5 — allow pending connections from concurrent AJAX queues
    httpsServer(nullptr),
    httpPort(port),
    httpsPort(httpsPort) {
  
  OTF_DEBUG("Initializing Esp32LocalServer (single-client, stateless)\n");
  OTF_DEBUG("  HTTP port: %d, HTTPS port: %d\n", httpPort, httpsPort);
  
  if (httpsPort == 0) return;

  // Certificate must be provided by firmware via otf_custom_cert_data/otf_custom_key_data
  if (!otf_custom_cert_data || !otf_custom_key_data) {
    OTF_DEBUG("  No certificate data provided, HTTPS disabled\n");
    return;
  }

  const unsigned char* cert = otf_custom_cert_data;
  uint16_t certLen = otf_custom_cert_len;
  const unsigned char* key = otf_custom_key_data;
  uint16_t keyLen = otf_custom_key_len;

  OTF_DEBUG("  Using certificate (%d bytes cert, %d bytes key)\n", certLen, keyLen);

  httpsServer = new WiFiSecureServer(httpsPort, cert, certLen, key, keyLen);
}

Esp32LocalServer::~Esp32LocalServer() {
  if (currentClient) {
    currentClient->stop();
    delete currentClient;
    currentClient = nullptr;
  }
  if (httpsServer) {
    delete httpsServer;
    httpsServer = nullptr;
  }
}

void Esp32LocalServer::begin() {
  httpServer.begin();
  OTF_DEBUG("HTTP server listening on port %d\n", httpPort);
  
  if (httpsServer) {
    if (httpsServer->begin()) {
      OTF_DEBUG("HTTPS server listening on port %d\n", httpsPort);
    } else {
      OTF_DEBUG("WARNING: HTTPS server failed to start\n");
    }
  }
}

void Esp32LocalServer::stop() {
  if (currentClient) {
    currentClient->stop();
    delete currentClient;
    currentClient = nullptr;
  }
  httpServer.end();
  OTF_DEBUG("HTTP server on port %d stopped\n", httpPort);
  if (httpsServer) {
    httpsServer->end();
    OTF_DEBUG("HTTPS server on port %d stopped\n", httpsPort);
  }
}

LocalClient *Esp32LocalServer::acceptClient() {
  // Close and delete the previous client — fully stateless.
  if (currentClient) {
    currentClient->stop();
    delete currentClient;
    currentClient = nullptr;
  }
  
  // Try HTTP first
  WiFiClient httpClient = httpServer.accept();
  if (httpClient) {
    currentRequestIsHttps = false;
    currentClient = new Esp32HttpClient(httpClient);
    return currentClient;
  }

  // Try HTTPS
  if (httpsServer) {    
    WiFiClient wifiClient = httpsServer->accept();
    if (wifiClient) {
      currentRequestIsHttps = true;
      Esp32HttpsClient* httpsClient = new Esp32HttpsClient(wifiClient, httpsServer);
      if (!httpsClient->isUsable()) {
        delete httpsClient;
        return nullptr;
      }
      currentClient = httpsClient;
      return currentClient;
    }
  }

  return nullptr;
}

// ============================================================================
// Esp32HttpClient Implementation (HTTP without SSL)
// ============================================================================

Esp32HttpClient::Esp32HttpClient(WiFiClient wifiClient) 
  : client(wifiClient), isActive(true) {
  client.setNoDelay(true);
}

OTF::Esp32HttpClient::~Esp32HttpClient() {
  if (isActive) {
    stop();
  }
}

bool OTF::Esp32HttpClient::dataAvailable() {
  return client.available();
}

size_t OTF::Esp32HttpClient::readBytes(char *buffer, size_t length) {
  return client.readBytes(buffer, length);
}

size_t OTF::Esp32HttpClient::readBytesUntil(char terminator, char *buffer, size_t length) {
  return client.readBytesUntil(terminator, buffer, length);
}

void OTF::Esp32HttpClient::print(const char *data) {
  client.print(data);
}

void OTF::Esp32HttpClient::print(const __FlashStringHelper *data) {
  client.print(data);
}

size_t OTF::Esp32HttpClient::write(const char *buffer, size_t length) {
  OTF_DEBUG("HTTP write: %d bytes\n", length);
  OTF_DEBUG("Content: %.*s\n", length, buffer);
  return client.write((const uint8_t *)buffer, length);
}

int OTF::Esp32HttpClient::peek() {
  return client.peek();
}

void OTF::Esp32HttpClient::setTimeout(int timeout) {
  client.setTimeout(timeout);
}

void OTF::Esp32HttpClient::flush() {
  // No-op.
  // NOTE: Arduino Client::flush() is often implemented as "discard received data".
  // Response streaming uses flush as a "push out" hint; clearing RX here can block
  // and severely slow down large streamed responses.
}

void OTF::Esp32HttpClient::stop() {
  client.stop();
  isActive = false;
}

// ============================================================================
// Esp32HttpsClient Implementation (HTTPS with SSL/TLS)
// ============================================================================

OTF::Esp32HttpsClient::Esp32HttpsClient(WiFiClient wifiClient, WiFiSecureServer* httpsServer)
  : client(wifiClient), isActive(true), ssl(nullptr), server(httpsServer)
{
  OTF_DEBUG("HTTPS client init\n");
  client.setNoDelay(true);
  client.setTimeout((int)timeoutMs);

  // Create SSL connection with handshake
  ssl = httpsServer->handshakeSSL(&client);
  if (!ssl) isActive = false;
}

OTF::Esp32HttpsClient::~Esp32HttpsClient() {
  OTF_DEBUG("~Esp32HttpsClient\n");
  if (ssl && isActive) {
    // Best-effort close_notify (100ms max — don't block the main loop)
    const uint32_t closeTimeoutMs = 100;
    uint32_t start = millis();
    while (true) {
      int r = mbedtls_ssl_close_notify(ssl);
      if (r == 0) break;
      if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
        if ((millis() - start) >= closeTimeoutMs) break;
        delay(1);
        continue;
      }
      break;
    }
  }
  client.stop();
  if (ssl && server) {
    server->returnSSLContext(ssl);  // Return to pool for reuse
  } else if (ssl) {
    mbedtls_ssl_free(ssl);
    delete ssl;
  }
  ssl = nullptr;
  isActive = false;
}

bool OTF::Esp32HttpsClient::dataAvailable() {
  if (!ssl) return false;
  return mbedtls_ssl_get_bytes_avail(ssl) > 0 || client.available();
}

size_t OTF::Esp32HttpsClient::readBytes(char *buffer, size_t length) {
  if (!ssl || !buffer || length == 0) return 0;
  uint32_t start = millis();
  size_t total = 0;
  while (total < length) {
    int r = mbedtls_ssl_read(ssl, (unsigned char*)buffer + total, length - total);
    if (r > 0) {
      total += (size_t)r;
      continue;
    }
    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
      if ((millis() - start) >= timeoutMs) break;
      delay(1);
      continue;
    }
    break;
  }
  return total;
}

size_t OTF::Esp32HttpsClient::readBytesUntil(char terminator, char *buffer, size_t length) {
  if (!ssl || !buffer || length == 0) return 0;
  uint32_t start = millis();
  size_t index = 0;
  while (index < length) {
    unsigned char c;
    int r = mbedtls_ssl_read(ssl, &c, 1);
    if (r > 0) {
      if ((char)c == terminator) break;
      buffer[index++] = (char)c;
      continue;
    }
    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
      if ((millis() - start) >= timeoutMs) break;
      delay(1);
      continue;
    }
    break;
  }
  return index;
}

void OTF::Esp32HttpsClient::print(const char *data) {
  if (!data) return;
  write(data, strlen(data));
}

void OTF::Esp32HttpsClient::print(const __FlashStringHelper *data) {
  const char* p = reinterpret_cast<const char*>(data);
  write(p, strlen(p));
}

size_t OTF::Esp32HttpsClient::write(const char *buffer, size_t length) {
  if (!ssl || !buffer || length == 0) return 0;
  uint32_t start = millis();
  size_t total = 0;
  while (total < length) {
    int w = mbedtls_ssl_write(ssl, (const unsigned char*)buffer + total, length - total);
    if (w > 0) {
      total += (size_t)w;
      continue;
    }
    if (w == MBEDTLS_ERR_SSL_WANT_READ || w == MBEDTLS_ERR_SSL_WANT_WRITE) {
      if ((millis() - start) >= timeoutMs) break;
      delay(1);
      continue;
    }
    break;
  }
  uint32_t elapsed = millis() - start;
  if (elapsed > 50) {
    OTF_DEBUG("HTTPS write slow: %u ms for %d bytes (sent %d)\n", elapsed, (int)length, (int)total);
  }
  return total;
}

int OTF::Esp32HttpsClient::peek() {
  return -1;  // Not supported for SSL
}

void OTF::Esp32HttpsClient::setTimeout(int timeout) {
  if (timeout < 0) timeout = 0;
  timeoutMs = (uint32_t)timeout;
  client.setTimeout(timeout);
}

void OTF::Esp32HttpsClient::flush() {
  // No-op.
  // For TLS, writes are pushed via mbedtls_ssl_write(); there is no separate
  // outbound flush. Avoid draining/clearing RX here because Response streaming
  // can call flush frequently.
}

void OTF::Esp32HttpsClient::stop() {
  OTF_DEBUG("stop HTTPS client\n");
  uint32_t stopStart = millis();
  if (ssl && isActive) {
    // Best-effort close_notify: 100ms max to avoid blocking OTF main loop.
    const uint32_t closeTimeoutMs = 100;
    uint32_t start = millis();
    while (true) {
      int r = mbedtls_ssl_close_notify(ssl);
      if (r == 0) break;
      if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
        if ((millis() - start) >= closeTimeoutMs) break;
        delay(1);
        continue;
      }
      break;
    }
  }
  if (ssl && server) {
    server->returnSSLContext(ssl);  // Return to pool
  } else if (ssl) {
    mbedtls_ssl_free(ssl);
    delete ssl;
  }
  ssl = nullptr;
  client.stop();
  isActive = false;
  OTF_DEBUG("SSL stop in %ums, heap=%d\n", (unsigned)(millis() - stopStart), ESP.getFreeHeap());
}

#endif
