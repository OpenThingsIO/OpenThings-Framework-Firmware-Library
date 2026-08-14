#ifndef OTF_FORWARDEDREQUEST_H
#define OTF_FORWARDEDREQUEST_H

#include <stddef.h>
#include <stdint.h>

namespace OTF {

struct ForwardedRequest {
  char *requestId = nullptr;
  char *requestData = nullptr;
  size_t requestLength = 0;
};

/**
 * Validate and split an OTC "FWD: <id>\r\n<request>" frame in place.
 * The carriage return following the four-byte request ID is replaced with a
 * null terminator only after the complete envelope has been validated.
 */
bool parseForwardedRequest(uint8_t *payload, size_t length, ForwardedRequest &request);

} // namespace OTF

#endif
