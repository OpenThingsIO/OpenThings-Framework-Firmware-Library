#ifndef OTF_ESP32LOCALSERVER_CONFIG_H
#define OTF_ESP32LOCALSERVER_CONFIG_H

/**
 * @file Esp32LocalServer_Config.h
 * @brief Configuration for OpenThings Framework ESP32 local server
 * 
 * Single-client, stateless design. No connection pool.
 */

/** SSL/TLS handshake timeout (milliseconds) */
#ifndef OTF_SSL_HANDSHAKE_TIMEOUT_MS
  #define OTF_SSL_HANDSHAKE_TIMEOUT_MS 5000
#endif

#endif /* OTF_ESP32LOCALSERVER_CONFIG_H */
