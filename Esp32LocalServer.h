#if defined(ESP32)
#ifndef OTF_ESP32LOCALSERVER_H
#define OTF_ESP32LOCALSERVER_H

#include "LocalServer.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
#include <mbedtls/ssl_ticket.h>
#endif

// Global pointers for custom certificate override.
// Must be set by the firmware before OTF construction.
extern const unsigned char* otf_custom_cert_data;
extern uint16_t otf_custom_cert_len;
extern const unsigned char* otf_custom_key_data;
extern uint16_t otf_custom_key_len;

// Single-client stateless design: accept one connection, handle it, close it.

namespace OTF {

// WiFiSecureServer: SSL/TLS wrapper für WiFiServer mit mbedTLS
class WiFiSecureServer {
  friend class Esp32HttpsClient; 
private:
  WiFiServer server;
  mbedtls_ssl_config sslConf;
  mbedtls_x509_crt serverCert;
  mbedtls_pk_context serverKey;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctrDrbg;
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
  mbedtls_ssl_ticket_context ticketCtx;
  bool ticketCtxInitialized;
#endif
  // SSL context pool for reuse (avoids new/delete per connection)
  static constexpr uint8_t SSL_CTX_POOL_SIZE = 3;
  mbedtls_ssl_context* sslPool[SSL_CTX_POOL_SIZE];
  bool sslPoolInUse[SSL_CTX_POOL_SIZE];
  uint16_t port;
  const unsigned char* certData;
  uint16_t certLength;
  const unsigned char* keyData;
  uint16_t keyLength;
  bool initialized;

  bool setupSSLContext();
  bool setupCertificate();

public:
  WiFiSecureServer(uint16_t port, const unsigned char* cert, uint16_t certLen, const unsigned char* key, uint16_t keyLen);
  ~WiFiSecureServer();
  
  bool begin();
  void end();
  WiFiClient accept();
  mbedtls_ssl_context* handshakeSSL(WiFiClient* wifiClient);
  void returnSSLContext(mbedtls_ssl_context* ssl);  // Return SSL context to pool
  
  mbedtls_ssl_config* getSSLConfig() { return &sslConf; }
};

  // HTTP Client (non-secure)
  class Esp32HttpClient : public LocalClient {
    friend class Esp32LocalServer;

  private:
    WiFiClient client;
    bool isActive;

    Esp32HttpClient(WiFiClient wifiClient);
    ~Esp32HttpClient();

  public:
    bool dataAvailable();
    size_t readBytes(char *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    void print(const char *data);
    void print(const __FlashStringHelper *data);
    size_t write(const char *buffer, size_t length);
    int peek();
    void setTimeout(int timeout);
    void flush();
    void stop();
    bool connected() { return client.connected(); }
  };

  // HTTPS Client (secure with SSL/TLS)
  class Esp32HttpsClient : public LocalClient {
    friend class Esp32LocalServer;

  private:
    WiFiClient client;           // Base WiFiClient
    mbedtls_ssl_context *ssl;
    WiFiSecureServer *server;      // For returning SSL ctx to pool
    bool isActive;
    // Used for mbedTLS read/write retry loops.
    uint32_t timeoutMs = 5000;

    Esp32HttpsClient(WiFiClient wifiClient, WiFiSecureServer *httpsServer);
    ~Esp32HttpsClient();

  public:
    bool isUsable() const { return ssl != nullptr && isActive; }
    bool dataAvailable();
    size_t readBytes(char *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    void print(const char *data);
    void print(const __FlashStringHelper *data);
    size_t write(const char *buffer, size_t length);
    int peek();
    void setTimeout(int timeout);
    void flush();
    void stop();
    bool connected() { return client.connected(); }
  };

  class Esp32LocalServer : public LocalServer {
  private:
    WiFiServer httpServer;              // HTTP server on port 80
    WiFiSecureServer* httpsServer;      // HTTPS server on port 443 with SSL/TLS
    LocalClient *currentClient = nullptr;
    
    uint16_t httpPort;
    uint16_t httpsPort;
    bool currentRequestIsHttps = false;

  public:
    Esp32LocalServer(uint16_t port = 80, uint16_t httpsPort = 443);
    ~Esp32LocalServer();

    LocalClient *acceptClient();
    void begin();
    void stop();
    bool isCurrentRequestHttps() const override { return currentRequestIsHttps; }
  };
}// namespace OTF

#endif
#endif
