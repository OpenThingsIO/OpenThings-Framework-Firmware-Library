## OpenThings Framework - Controller Library

This library provides a single interface that can be used to handle HTTP requests made directly to the controller or forwarded to the controller via a websocket.

### Required Libraries

* [https://github.com/Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) (With PlatformIO it will automatically install this).

### Installation

The easiest way to use the library is to place it in your Arduino libraries directory, which is automatically included by the Arduino IDE and can manually be included by any other project that doesn't use the IDE.
First use the Arduino IDE to install the `WebSockets` library v2.7.2 or later by Markus Sattler.
Then, copy this library into the `libraries` directory of your sketchbook location (which can be viewed from the Arduino IDE preferences).

### Request Bodies

Local HTTP request bodies require a valid `Content-Length` and must arrive completely within the request timeout. The default maximum body size is 8192 bytes. Resource-constrained applications can override it at compile time, for example with `-DOTF_MAX_BODY_SIZE=2048`.

### Tests

Run the focused native parser tests with:

```sh
make -C test test
```

### TODO

* Add support for OTA firmware updates.

* Cleanup debug logging.
