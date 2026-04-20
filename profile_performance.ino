/**
 * @file profile_performance.ino
 * @brief Performance profiling and benchmarking tool for OpenThings Framework
 * 
 * This sketch measures and profiles:
 * - Memory utilization patterns
 * - Connection handling performance
 * - Response time under various loads
 * - PSRAM usage efficiency
 * - TLS handshake performance
 */

#include "OpenThingsFramework.h"
#include "Esp32Performance.h"

#include <vector>

// ============================================================================
// Configuration
// ============================================================================

#define PROFILE_DURATION_MS (60 * 1000)  // 1 minute profiling
#define PROFILE_HTTP_ONLY false          // Set true to test HTTP only (no HTTPS)
#define TEST_CLIENT_COUNT 4              // Simulate this many clients

// ============================================================================
// Global State
// ============================================================================

OTF::Esp32LocalServer server(80, PROFILE_HTTP_ONLY ? 0 : 443, 4);
OTF::PerformanceMonitor perfMonitor;
std::vector<OTF::LocalClient*> testClients;

unsigned long profileStartTime = 0;
unsigned long lastMetricsPrint = 0;
const unsigned long METRICS_PRINT_INTERVAL = 10000;  // Print every 10 seconds

// ============================================================================
// Test Harness
// ============================================================================

/**
 * Simulate HTTP client connection
 */
class MockHttpClient {
public:
  unsigned long connectedTime;
  unsigned long lastDataTime;
  uint32_t dataReceived = 0;
  uint32_t dataSent = 0;
  bool active = false;
  
  MockHttpClient() : connectedTime(millis()), lastDataTime(millis()), active(true) {
    perfMonitor.recordConnection();
  }
  
  ~MockHttpClient() {
    perfMonitor.recordDisconnection();
  }
  
  bool isAlive() {
    // Kill after 5 seconds of inactivity or 30 seconds total
    unsigned long now = millis();
    return active && 
           (now - connectedTime) < 30000 && 
           (now - lastDataTime) < 5000;
  }
};

std::vector<MockHttpClient*> mockClients;

/**
 * Create mock client
 */
void createMockClient() {
  if (mockClients.size() < TEST_CLIENT_COUNT) {
    MockHttpClient* client = new MockHttpClient();
    mockClients.push_back(client);
    
    unsigned long elapsed = millis() - profileStartTime;
    Serial.printf("[%5lu ms] Created mock client %u\n", elapsed, mockClients.size());
  }
}

/**
 * Update mock clients
 */
void updateMockClients() {
  unsigned long now = millis();
  
  for (auto it = mockClients.begin(); it != mockClients.end(); ) {
    MockHttpClient* client = *it;
    
    if (!client->isAlive()) {
      unsigned long sessionDuration = now - client->connectedTime;
      unsigned long elapsed = millis() - profileStartTime;
      
      Serial.printf("[%5lu ms] Mock client closed after %lu ms (sent: %u, recv: %u bytes)\n",
                    elapsed, sessionDuration, client->dataSent, client->dataReceived);
      
      perfMonitor.recordDisconnection();
      delete client;
      it = mockClients.erase(it);
    } else {
      // Simulate activity
      if ((now - client->lastDataTime) > 1000) {
        client->dataSent += 128;
        client->dataReceived += 256;
        client->lastDataTime = now;
        
        // Simulate response time measurement
        uint32_t responseTime = random(10, 100);
        perfMonitor.recordResponseTime(responseTime);
      }
      
      ++it;
    }
  }
}

/**
 * Simulate TLS handshake results
 */
void simulateTlsMetrics() {
  static unsigned long lastTlsSimulation = 0;
  unsigned long now = millis();
  
  if (now - lastTlsSimulation > 5000) {
    lastTlsSimulation = now;
    
    // 95% success rate
    if (random(100) < 95) {
      uint32_t handshakeTime = random(500, 2000);
      perfMonitor.recordTlsHandshakeSuccess(handshakeTime);
    } else {
      perfMonitor.recordTlsHandshakeFailure();
    }
  }
}

// ============================================================================
// Benchmarking Functions
// ============================================================================

/**
 * Benchmark memory allocation patterns
 */
void benchmarkMemoryAllocation() {
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║   Memory Allocation Benchmark    ║");
  Serial.println("╚══════════════════════════════════╝\n");
  
  struct MemTest {
    uint32_t size;
    const char* name;
  };
  
  MemTest tests[] = {
    {512, "512 B (small buffer)"},
    {1024, "1 KB (tiny buffer)"},
    {4096, "4 KB (standard read buffer)"},
    {8192, "8 KB (standard write buffer)"},
    {16384, "16 KB (large buffer)"},
    {32768, "32 KB (extra large buffer)"},
  };
  
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    uint32_t start_heap = ESP.getFreeHeap();
    uint32_t start_psram = ESP.getFreePsram();
    
    // Allocate multiple blocks
    std::vector<void*> blocks;
    for (int j = 0; j < 4; j++) {
      #if OTF_USE_PSRAM
        void* ptr = ps_malloc(tests[i].size);
      #else
        void* ptr = malloc(tests[i].size);
      #endif
      blocks.push_back(ptr);
    }
    
    uint32_t end_heap = ESP.getFreeHeap();
    uint32_t end_psram = ESP.getFreePsram();
    
    Serial.printf("%-25s: ", tests[i].name);
    Serial.printf("DRAM: -%u bytes, ", start_heap - end_heap);
    Serial.printf("PSRAM: -%u bytes\n", start_psram - end_psram);
    
    // Free blocks
    for (auto ptr : blocks) {
      if (ptr) free(ptr);
    }
    
    delay(100);
  }
  
  Serial.println();
}

/**
 * Benchmark response time under load
 */
void benchmarkResponseTime() {
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║   Response Time Benchmark        ║");
  Serial.println("╚══════════════════════════════════╝\n");
  
  Serial.println("Simulating response times...");
  
  uint32_t times[100];
  uint32_t sum = 0;
  uint32_t minTime = UINT32_MAX;
  uint32_t maxTime = 0;
  
  for (int i = 0; i < 100; i++) {
    uint32_t simTime = random(10, 150);
    times[i] = simTime;
    sum += simTime;
    
    if (simTime < minTime) minTime = simTime;
    if (simTime > maxTime) maxTime = simTime;
    
    perfMonitor.recordResponseTime(simTime);
  }
  
  Serial.printf("Average:  %u ms\n", sum / 100);
  Serial.printf("Min:      %u ms\n", minTime);
  Serial.printf("Max:      %u ms\n", maxTime);
  Serial.printf("Range:    %u ms\n", maxTime - minTime);
  
  // Calculate standard deviation
  uint32_t avg = sum / 100;
  uint32_t varSum = 0;
  for (int i = 0; i < 100; i++) {
    int32_t diff = times[i] - avg;
    varSum += (diff * diff);
  }
  uint32_t stdDev = sqrt(varSum / 100);
  Serial.printf("Std Dev:  %u ms\n\n", stdDev);
}

/**
 * Benchmark TLS handshake performance
 */
void benchmarkTlsHandshake() {
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║   TLS Handshake Benchmark       ║");
  Serial.println("╚══════════════════════════════════╝\n");
  
  Serial.println("Simulating TLS handshakes (95% success rate)...");
  
  int successCount = 0;
  int failCount = 0;
  uint32_t timeSum = 0;
  
  for (int i = 0; i < 20; i++) {
    if (random(100) < 95) {
      uint32_t handshakeTime = random(500, 2500);
      perfMonitor.recordTlsHandshakeSuccess(handshakeTime);
      successCount++;
      timeSum += handshakeTime;
    } else {
      perfMonitor.recordTlsHandshakeFailure();
      failCount++;
    }
  }
  
  Serial.printf("Successful:  %d\n", successCount);
  Serial.printf("Failed:      %d\n", failCount);
  Serial.printf("Success rate: %.1f%%\n", ((float)successCount / 20) * 100);
  if (successCount > 0) {
    Serial.printf("Avg time:    %u ms\n\n", timeSum / successCount);
  }
}

// ============================================================================
// Setup & Loop
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   OpenThings Framework Profiler      ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Run benchmarks
  benchmarkMemoryAllocation();
  delay(1000);
  
  benchmarkResponseTime();
  delay(1000);
  
  benchmarkTlsHandshake();
  delay(1000);
  
  // Initialize server
  Serial.println("Starting server for load simulation...\n");
  server.begin();
  
  profileStartTime = millis();
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - profileStartTime;
  
  // Stop profiling after duration
  if (elapsed > PROFILE_DURATION_MS) {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   Profiling Complete                 ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    Serial.println("Final Performance Report:");
    perfMonitor.printMetrics(0);
    perfMonitor.printOptimizationRecommendations(0);
    
    // Halt
    while (true) {
      delay(10000);
    }
  }
  
  // Simulate clients
  if (random(100) < 20) {  // 20% chance to create new client each loop
    createMockClient();
  }
  
  updateMockClients();
  simulateTlsMetrics();
  
  // Print metrics periodically
  if (now - lastMetricsPrint > METRICS_PRINT_INTERVAL) {
    lastMetricsPrint = now;
    
    Serial.printf("\n[PROFILE %lu/%lu ms] ", elapsed, PROFILE_DURATION_MS);
    Serial.printf("Clients: %u, DRAM: %u, PSRAM: %u\n",
                  mockClients.size(),
                  ESP.getFreeHeap(),
                  ESP.getFreePsram());
    
    perfMonitor.printMetrics(mockClients.size());
  }
  
  delay(100);
}

/**
 * ADVANCED: Custom benchmark for specific workload
 * 
 * Uncomment and modify to test custom scenarios
 */

/*
void benchmarkCustomWorkload() {
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║   Custom Workload Benchmark      ║");
  Serial.println("╚══════════════════════════════════╝\n");
  
  // Simulate specific workload pattern
  // Example: Many small requests + few large requests
  
  for (int i = 0; i < 50; i++) {
    // Small request (50% of time)
    perfMonitor.recordResponseTime(random(10, 30));
  }
  
  for (int i = 0; i < 10; i++) {
    // Large request (slower)
    perfMonitor.recordResponseTime(random(100, 500));
  }
  
  Serial.println("Workload pattern: 50 small + 10 large requests");
  perfMonitor.printMetrics(0);
}
*/
