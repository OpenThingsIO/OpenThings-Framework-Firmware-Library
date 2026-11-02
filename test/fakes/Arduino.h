#ifndef OTF_TEST_ARDUINO_H
#define OTF_TEST_ARDUINO_H

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <functional>
#include <string>
#include <algorithm>

class __FlashStringHelper;
using String = std::string;
using std::min;
#define F(value) reinterpret_cast<const __FlashStringHelper *>(value)
#define PSTR(value) (value)
#define snprintf_P snprintf
#define strncpy_P strncpy
#define strcmp_P strcmp
#define strncmp_P strncmp
#define strlen_P strlen
#define memcpy_P memcpy

uint32_t millis();
long secureRandom(long maximum);
struct FakeESP {
  uint32_t chipId = 0x123456;
  uint64_t mac = 0xaabbcc123456ULL;
  uint32_t getChipId() const { return chipId; }
  uint64_t getEfuseMac() const { return mac; }
};
extern FakeESP ESP;
inline void delay(unsigned long) {}

#endif
