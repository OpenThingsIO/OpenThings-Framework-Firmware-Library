## OpenThings Framework - Controller Library

This library provides a single interface that can be used to handle HTTP requests made directly to the controller or forwarded to the controller via a websocket.

### Required Libraries

* [[https://github.com/gilmaimon/ArduinoWebsockets](https://github.com/Links2004/arduinoWebSockets/)](links2004/WebSockets) (With PlatformIO it will automatically install this).

### Installation

The easiest way to use the library is to place it in your Arduino libraries directory, which is automatically included by the Arduino IDE and can manually be included by any other project that doesn't use the IDE.
First use the Arduino IDE to install the library `WebSockets` v2.4.1 by Markus Sattler.
Then, copy the directory `OTF-Controller-Library` into the `libraries` directory of your sketchbook location (which can be viewed from the Arduino IDE preferences).

### TODO

* Add support for OTA firmware updates.

* Cleanup debug logging.

* Testing.
