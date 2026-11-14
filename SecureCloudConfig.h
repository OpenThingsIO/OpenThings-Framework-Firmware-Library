#pragma once

// This opt-in is coupled to OpenGarage's reviewed ESP8266 transport, not a
// generic promise that upstream WebSockets verifies certificates on all targets.
#if defined(OG_SECURE_CLOUD) && (!defined(ARDUINO) || !defined(ESP8266))
#error "OG_SECURE_CLOUD requires the reviewed Arduino ESP8266 integration"
#endif
