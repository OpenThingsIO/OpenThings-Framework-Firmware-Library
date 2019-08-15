#ifndef OTF_OPENTHINGSFRAMEWORK_H
#define OTF_OPENTHINGSFRAMEWORK_H

#include "Request.h"
#include "Response.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

namespace OTF {
    typedef Response (*callback_t)(const Request &request);

    class OpenThingsFramework {
    private:
        WiFiServer server;
        WiFiClient wifiClient;
        WebSocketsClient webSocket;
        LinkedMap<callback_t> callbacks;
        callback_t missingPageCallback;

        void webSocketCallback(WStype_t type, uint8_t *payload, size_t length);

        Response getResponse(const Request &req);
        void localServerLoop();

        static Response defaultMissingPageCallback(const Request &req);

    public:
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
