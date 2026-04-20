/**
 * @file example_multiclient_server.ino
 * @brief Example demonstrating multi-client server with PSRAM optimization
 * 
 * This example shows how to use the enhanced OpenThings Framework with:
 * - Multiple concurrent connections
 * - PSRAM buffering for improved performance
 * - Connection pool management
 * - Proper client lifecycle handling
 */

#include "OpenThingsFramework.h"
#include <vector>

// ============================================================================
// Configuration
// ============================================================================

#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"
#define HTTP_PORT 80
#define HTTPS_PORT 443

// ============================================================================
// Global Objects
// ============================================================================

OTF::Esp32LocalServer server(HTTP_PORT, HTTPS_PORT, 4);  // Max 4 concurrent clients
std::vector<OTF::LocalClient*> activeClients;

unsigned long lastStatusPrint = 0;
const unsigned long STATUS_PRINT_INTERVAL = 5000;  // Print status every 5 seconds

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Print memory information
 */
void printMemoryStats() {
  Serial.println("\n=== Memory Statistics ===");
  Serial.printf("Free DRAM: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Largest DRAM block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("PSRAM Available: %s\n", psramFound() ? "Yes" : "No");
  Serial.printf("Active clients: %u\n", server.getActiveClientCount());
  Serial.println("==========================\n");
}

/**
 * Initialize WiFi connection
 */
bool initializeWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi");
    return false;
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("HTTPS available at: https://");
  Serial.println(WiFi.localIP());
  
  return true;
}

/**
 * Create a simple HTTP response
 */
void sendHttpResponse(OTF::LocalClient* client, int statusCode, const char* contentType) {
  // Status line
  client->print("HTTP/1.1 ");
  client->print(statusCode);
  client->print(" OK\r\n");
  
  // Headers
  client->print("Content-Type: ");
  client->print(contentType);
  client->print("\r\n");
  client->print("Content-Length: ");
  
  // Calculate content length
  String body = "{ \"status\": \"ok\", \"clients\": ";
  body += server.getActiveClientCount();
  body += ", \"protocol\": \"";
  body += server.isCurrentRequestHttps() ? "HTTPS" : "HTTP";
  body += "\" }";
  
  client->print(body.length());
  client->print("\r\n");
  client->print("Connection: close\r\n");
  client->print("Access-Control-Allow-Origin: *\r\n");
  client->print("\r\n");
  
  // Body
  client->print(body.c_str());
  client->flush();
}

/**
 * Process incoming HTTP request
 */
void processHttpRequest(OTF::LocalClient* client) {
  if (!client) return;
  
  char buffer[512];
  
  // Read request line
  size_t bytesRead = client->readBytesUntil('\n', buffer, sizeof(buffer) - 1);
  if (bytesRead == 0) return;
  
  buffer[bytesRead] = '\0';
  
  // Simple HTTP response
  if (strstr(buffer, "GET")) {
    Serial.println("GET request received");
    sendHttpResponse(client, 200, "application/json");
  } else {
    Serial.println("Non-GET request received");
    sendHttpResponse(client, 405, "text/plain");
  }
}

/**
 * Accept new clients and add to active list
 */
void acceptNewClients() {
  // Try to accept a new client without blocking
  OTF::LocalClient* newClient = server.acceptClientNonBlocking();
  
  if (newClient) {
    Serial.printf("[CLIENT ACCEPTED] Total active: %u\n", server.getActiveClientCount());
    Serial.printf("Connection type: %s\n", server.isCurrentRequestHttps() ? "HTTPS" : "HTTP");
    activeClients.push_back(newClient);
  }
}

/**
 * Clean up inactive/disconnected clients
 */
void cleanupInactiveClients() {
  for (auto it = activeClients.begin(); it != activeClients.end(); ) {
    OTF::LocalClient* client = *it;
    
    if (!client) {
      it = activeClients.erase(it);
      continue;
    }
    
    // Check if client has disconnected
    // In a real scenario, you'd implement proper connection checking
    // For now, we'll rely on client::dataAvailable() returning false for dead connections
    
    ++it;
  }
}

/**
 * Process all active clients
 */
void processAllClients() {
  for (auto it = activeClients.begin(); it != activeClients.end(); ) {
    OTF::LocalClient* client = *it;
    
    if (!client) {
      it = activeClients.erase(it);
      continue;
    }
    
    // Check if client has data available
    if (client->dataAvailable()) {
      processHttpRequest(client);
      
      // Close client after processing
      client->stop();
      delete client;
      it = activeClients.erase(it);
      Serial.printf("[CLIENT CLOSED] Remaining active: %u\n", activeClients.size());
    } else {
      ++it;
    }
  }
}

// ============================================================================
// Arduino Setup & Loop
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== OpenThings Framework Multi-Client Server ===\n");
  
  // Initialize WiFi
  if (!initializeWiFi()) {
    Serial.println("Failed to initialize WiFi. Entering low-power mode.");
    while (true) {
      delay(10000);
    }
  }
  
  // Print memory stats
  printMemoryStats();
  
  // Initialize and start server
  Serial.println("Starting HTTP/HTTPS server...");
  server.begin();
  Serial.println("Server started successfully!\n");
}

void loop() {
  // Accept new incoming connections
  acceptNewClients();
  
  // Process all active clients
  processAllClients();
  
  // Clean up inactive clients
  cleanupInactiveClients();
  
  // Print status periodically
  if (millis() - lastStatusPrint > STATUS_PRINT_INTERVAL) {
    lastStatusPrint = millis();
    
    Serial.print("\n[STATUS] Active clients: ");
    Serial.print(server.getActiveClientCount());
    Serial.print(", Free DRAM: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes, Free PSRAM: ");
    Serial.print(ESP.getFreePsram());
    Serial.println(" bytes");
  }
  
  // Small delay to prevent watchdog timeout
  delay(10);
}

/**
 * ADVANCED EXAMPLE: Using the new OpenThings Framework API
 * Uncomment to use instead of the basic example above
 */

/*
// Advanced multi-client handling with proper lifecycle management
void loop_advanced() {
  // 1. Accept new clients
  OTF::LocalClient* newClient = server.acceptClientNonBlocking();
  if (newClient) {
    activeClients.push_back(newClient);
    Serial.printf("New %s client connected (total: %u)\n",
                  server.isCurrentRequestHttps() ? "HTTPS" : "HTTP",
                  activeClients.size());
  }
  
  // 2. Process all active clients with timeout tracking
  unsigned long now = millis();
  for (auto it = activeClients.begin(); it != activeClients.end(); ) {
    OTF::LocalClient* client = *it;
    
    // Implement client timeout logic
    if (client->dataAvailable()) {
      // Reset idle counter when data arrives
      processHttpRequest(client);
      client->stop();
      delete client;
      it = activeClients.erase(it);
    } else {
      ++it;
    }
  }
  
  // 3. Periodic status update
  static unsigned long lastLog = 0;
  if (now - lastLog > 5000) {
    lastLog = now;
    Serial.printf("Server status: %u clients, %u bytes free PSRAM\n",
                  server.getActiveClientCount(),
                  ESP.getFreePsram());
  }
  
  delay(10);
}
*/
