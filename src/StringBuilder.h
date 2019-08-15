#ifndef OTF_STRINGBUILDER_H
#define OTF_STRINGBUILDER_H

#include <Arduino.h>

namespace OTF {
    /**
     * Wraps a buffer to build a string with repeated calls to sprintf. If any of calls to sprintf cause an error (such
     * as exceeding the size of the internal buffer), the error will be silently swallowed and the StringBuilder will be
     * marked as invalid. This means that any error checking can occur after the entire string has been built instead of
     * a check being required after each individual call to sprintf.
     */
    class StringBuilder {
    private:
        size_t maxLength;
        char *buffer;
        size_t length = 0;
        bool valid = true;

    public:
        explicit StringBuilder(size_t maxLength);
        ~StringBuilder();

        /**
         * Inserts a string into the buffer at the current position using the same formatting rules as printf. If the operation
         * would cause the buffer length to be exceeded or some other error occurs, the StringBuilder will be marked as invalid.
         * @param format
         * @param ...
         */
        void printf(char *format, ...);

        char *toString() const;

        size_t getLength() const;

        /**
         * Returns a boolean indicating if the string was built successfully without any errors. If false, the behavior
         * of toString() is undefined, and the string it returns (which may or may not be null terminated) should NOT
         * be used.
         */
        bool isValid();
    };
}

#endif
