#ifndef OTF_ESP32_PERFORMANCE_H
#define OTF_ESP32_PERFORMANCE_H

/**
 * @file Esp32Performance.h
 * @brief Performance monitoring and optimization utilities for OpenThings Framework
 * 
 * Provides:
 * - Real-time performance metrics
 * - Memory usage tracking
 * - Connection performance monitoring
 * - Automatic optimization recommendations
 */

#include <Arduino.h>
#include <stdint.h>

namespace OTF {

/**
 * Performance Metrics Structure
 */
struct PerformanceMetrics {
  // Memory metrics
  uint32_t freeHeap;
  uint32_t freePsram;
  uint32_t largestFreeBlock;
  
  // Connection metrics
  uint16_t activeConnections;
  uint32_t totalConnectionsAccepted;
  uint32_t totalConnectionsClosed;
  
  // Timing metrics
  uint32_t avgResponseTime_ms;
  uint32_t maxResponseTime_ms;
  uint32_t minResponseTime_ms;
  
  // TLS metrics
  uint16_t tlsHandshakesSuccessful;
  uint16_t tlsHandshakesFailed;
  uint32_t avgTlsHandshakeTime_ms;
};

/**
 * @class PerformanceMonitor
 * @brief Real-time performance monitoring and optimization
 */
class PerformanceMonitor {
public:
  PerformanceMonitor() : 
    totalConnectionsAccepted(0),
    totalConnectionsClosed(0),
    tlsHandshakesSuccessful(0),
    tlsHandshakesFailed(0),
    responseTimeSum(0),
    responseTimeCount(0),
    minResponseTime(UINT32_MAX),
    maxResponseTime(0),
    tlsHandshakeTimeSum(0),
    tlsHandshakeCount(0),
    avgTlsHandshakeTime(0)
  {
  }
  
  /**
   * Record a new client connection
   */
  void recordConnection() {
    totalConnectionsAccepted++;
  }
  
  /**
   * Record client disconnection
   */
  void recordDisconnection() {
    totalConnectionsClosed++;
  }
  
  /**
   * Record successful TLS handshake with timing
   */
  void recordTlsHandshakeSuccess(uint32_t timeMs) {
    tlsHandshakesSuccessful++;
    tlsHandshakeTimeSum += timeMs;
    tlsHandshakeCount++;
    if (tlsHandshakeCount > 0) {
      avgTlsHandshakeTime = tlsHandshakeTimeSum / tlsHandshakeCount;
    }
  }
  
  /**
   * Record failed TLS handshake
   */
  void recordTlsHandshakeFailure() {
    tlsHandshakesFailed++;
  }
  
  /**
   * Record HTTP response time
   */
  void recordResponseTime(uint32_t timeMs) {
    responseTimeSum += timeMs;
    responseTimeCount++;
    
    if (timeMs < minResponseTime) minResponseTime = timeMs;
    if (timeMs > maxResponseTime) maxResponseTime = timeMs;
  }
  
  /**
   * Get current performance metrics
   */
  PerformanceMetrics getMetrics(uint16_t activeConnections) {
    PerformanceMetrics metrics = {};
    
    metrics.freeHeap = ESP.getFreeHeap();
    metrics.freePsram = ESP.getFreePsram();
    
    #ifdef heap_caps_get_largest_free_block
    metrics.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    #else
    metrics.largestFreeBlock = metrics.freeHeap;
    #endif
    
    metrics.activeConnections = activeConnections;
    metrics.totalConnectionsAccepted = totalConnectionsAccepted;
    metrics.totalConnectionsClosed = totalConnectionsClosed;
    
    if (responseTimeCount > 0) {
      metrics.avgResponseTime_ms = responseTimeSum / responseTimeCount;
      metrics.minResponseTime_ms = minResponseTime;
      metrics.maxResponseTime_ms = maxResponseTime;
    }
    
    metrics.tlsHandshakesSuccessful = tlsHandshakesSuccessful;
    metrics.tlsHandshakesFailed = tlsHandshakesFailed;
    metrics.avgTlsHandshakeTime_ms = avgTlsHandshakeTime;
    
    return metrics;
  }
  
  /**
   * Print formatted metrics to Serial
   */
  void printMetrics(uint16_t activeConnections) {
    PerformanceMetrics m = getMetrics(activeConnections);
    
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║     OpenThings Framework Performance      ║");
    Serial.println("╚════════════════════════════════════════════╝");
    
    Serial.println("\n[MEMORY]");
    Serial.printf("  Free DRAM:           %u bytes\n", m.freeHeap);
    Serial.printf("  Free PSRAM:          %u bytes\n", m.freePsram);
    Serial.printf("  Largest free block:  %u bytes\n", m.largestFreeBlock);
    float totalFree = m.freeHeap + m.freePsram;
    float totalMem = 320 + 4000;  // Approximate for ESP32
    Serial.printf("  Memory utilization:  %.1f%%\n", 100.0 - (totalFree/totalMem * 100.0));
    
    Serial.println("\n[CONNECTIONS]");
    Serial.printf("  Active:              %u\n", m.activeConnections);
    Serial.printf("  Total accepted:      %u\n", m.totalConnectionsAccepted);
    Serial.printf("  Total closed:        %u\n", m.totalConnectionsClosed);
    Serial.printf("  Uptime connections:  %.1f%%\n", 
                  m.totalConnectionsClosed > 0 ? 
                  ((float)m.totalConnectionsAccepted / (m.totalConnectionsAccepted + m.totalConnectionsClosed)) * 100 
                  : 100.0);
    
    Serial.println("\n[HTTP RESPONSE TIME]");
    Serial.printf("  Average:             %u ms\n", m.avgResponseTime_ms);
    Serial.printf("  Min/Max:             %u / %u ms\n", m.minResponseTime_ms, m.maxResponseTime_ms);
    Serial.printf("  Requests processed:  %u\n", responseTimeCount);
    
    Serial.println("\n[TLS/HTTPS]");
    Serial.printf("  Handshakes success:  %u\n", m.tlsHandshakesSuccessful);
    Serial.printf("  Handshakes failed:   %u\n", m.tlsHandshakesFailed);
    Serial.printf("  Avg handshake time:  %u ms\n", m.avgTlsHandshakeTime_ms);
    if (m.tlsHandshakesSuccessful > 0) {
      float successRate = ((float)m.tlsHandshakesSuccessful / 
                          (m.tlsHandshakesSuccessful + m.tlsHandshakesFailed)) * 100;
      Serial.printf("  Success rate:        %.1f%%\n", successRate);
    }
    
    Serial.println("\n╚════════════════════════════════════════════╝\n");
  }
  
  /**
   * Get optimization recommendations based on current metrics
   */
  void printOptimizationRecommendations(uint16_t activeConnections) {
    PerformanceMetrics m = getMetrics(activeConnections);
    
    Serial.println("\n[OPTIMIZATION RECOMMENDATIONS]");
    
    // Memory pressure check
    if (m.freeHeap < 50000) {
      Serial.println("⚠️  MEMORY PRESSURE: Free DRAM < 50KB");
      Serial.println("   → Reduce buffer sizes or max concurrent clients");
    }
    
    // Response time check
    if (m.avgResponseTime_ms > 100) {
      Serial.println("⚠️  SLOW RESPONSES: Average > 100ms");
      Serial.println("   → Check network latency or enable TCP_NODELAY");
      Serial.println("   → Consider reducing response payload size");
    }
    
    // TLS handshake check
    if (tlsHandshakesFailed > 0) {
      float failRate = ((float)tlsHandshakesFailed / 
                       (tlsHandshakesSuccessful + tlsHandshakesFailed)) * 100;
      if (failRate > 5.0) {
        Serial.printf("⚠️  TLS FAILURES: %.1f%% of handshakes failed\n", failRate);
        Serial.println("   → Check certificate validity");
        Serial.println("   → Increase SSL handshake timeout");
        Serial.println("   → Verify client TLS compatibility");
      }
    }
    
    // Connection starvation check
    if (m.activeConnections == 0 && m.totalConnectionsAccepted > 0) {
      Serial.println("ℹ️  All connections closed. Server idle.");
    }
    
    // Positive indicators
    if (m.freeHeap > 200000 && m.avgResponseTime_ms < 50) {
      Serial.println("✓ System running optimally");
    }
    
    Serial.println();
  }
  
  /**
   * Reset all metrics
   */
  void reset() {
    totalConnectionsAccepted = 0;
    totalConnectionsClosed = 0;
    tlsHandshakesSuccessful = 0;
    tlsHandshakesFailed = 0;
    responseTimeSum = 0;
    responseTimeCount = 0;
    minResponseTime = UINT32_MAX;
    maxResponseTime = 0;
    tlsHandshakeTimeSum = 0;
    tlsHandshakeCount = 0;
    avgTlsHandshakeTime = 0;
  }

private:
  uint32_t totalConnectionsAccepted;
  uint32_t totalConnectionsClosed;
  uint16_t tlsHandshakesSuccessful;
  uint16_t tlsHandshakesFailed;
  
  uint32_t responseTimeSum;
  uint32_t responseTimeCount;
  uint32_t minResponseTime;
  uint32_t maxResponseTime;
  
  uint32_t tlsHandshakeTimeSum;
  uint32_t tlsHandshakeCount;
  uint32_t avgTlsHandshakeTime;
};

} // namespace OTF

#endif /* OTF_ESP32_PERFORMANCE_H */
