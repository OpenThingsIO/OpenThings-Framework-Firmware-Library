#ifndef OTF_OPENTHINGSFRAMEWORK_H
#define OTF_OPENTHINGSFRAMEWORK_H

#include "Request.h"
#include "Response.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

namespace OTF {
    typedef void (*callback_t)(const Request &request, Response &response);

    class OpenThingsFramework {
    private:
        WiFiServer server;
        WiFiClient wifiClient;
        WebSocketsClient *webSocket = nullptr;
        LinkedMap<callback_t> callbacks;
        callback_t missingPageCallback;

        void webSocketCallback(WStype_t type, uint8_t *payload, size_t length);

        void fillResponse(const Request &req, Response &res);
        void localServerLoop();

        static void defaultMissingPageCallback(const Request &req, Response &res);

    public:
        /**
         * Initializes the library to only listen on a local webserver.
         * @param webServerPort The local port to bind the webserver to.
         */
        OpenThingsFramework(uint16_t webServerPort);

        /**
         * Initializes the library to listen on a local webserver and connect to a remote websocket.
         * @param webServerPort The local port to bind the webserver to.
         * @param webSocketHost The host of the remote websocket.
         * @param webSocketPort The port of the remote websocket.
         * @param deviceKey The unique device key that identifies this device.
         */
        OpenThingsFramework(uint16_t webServerPort, const String &webSocketHost, uint16_t webSocketPort,
                            const String &deviceKey);

        /**
         * Registers a callback function to run when a request is made to the specified path. The callback function will
         * be passed an OpenThingsRequest, and must return an OpenThingsResponse.
         * @param path
         * @param callback
         */
        void on(const char *path, callback_t callback, HTTPMethod method = HTTP_ANY);

        /**
         * Registers a callback function to run when a request is made to the specified path. The callback function will
         * be passed an OpenThingsRequest, and must return an OpenThingsResponse.
         * @param path
         * @param callback
         */
        void on(const __FlashStringHelper *path, callback_t callback, HTTPMethod method = HTTP_ANY);

        /** Registers a callback function to run when a request is received but its path does not match a registered callback. */
        void onMissingPage(callback_t callback);

        void loop();
    };
}

#endif
