## OpenThings Framework Controller Library

This library provides a single interface that can be used to handle HTTP requests made directly to the controller or forwarded to the controller via a websocket.

### Required Libraries

* https://github.com/Links2004/arduinoWebSockets

### TODO

* Update HTTP request parsing to handle multipart bodies.

* Pass a StringBuilder to callbacks instead of having them return a string (removes the need to copy the string).

* Cleanup debug logging.

* Testing.
