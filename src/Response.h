#ifndef OTF_RESPONSE_H
#define OTF_RESPONSE_H

#include "LinkedMap.h"
#include "Response.h"
#include "StringBuilder.h"

#include <Arduino.h>

// The maximum possible size of response messages.
#define RESPONSE_BUFFER_SIZE 8192

namespace OTF {

    class Response {
        friend class OpenThingsFramework;

    private:
        uint16_t statusCode;
        String body;
        LinkedMap<const char *> headers;
        /** The StringBuilder that will be used by the toString() method. */
        StringBuilder builder = StringBuilder(RESPONSE_BUFFER_SIZE);

        /** Returns a null terminated string of this response, or NULL if an error occurs while building the string. */
        char *toString();

    public:
        static const size_t MAX_RESPONSE_LENGTH = RESPONSE_BUFFER_SIZE;

        Response(uint16_t statusCode, const String &body);

        /** Sets a response header. If called multiple times for the same header name, the header will be specified
         * multiple times to create a list.
         */
        void setHeader(char *name, const char *value);

        /** Sets a response header. If called multiple times for the same header name, the header will be specified
         * multiple times to create a list.
         */
        void setHeader(const __FlashStringHelper *name, const char *value);

    };
}
#endif
