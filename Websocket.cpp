#include "Websocket.h"

void WebsocketClient::enableWebSocketHeartbeat(unsigned long interval, unsigned long timeout, unsigned long maxMissed) {
    webSocketHeartbeatInterval = interval;
    webSocketHeartbeatTimeout = timeout;
    webSocketHeartbeatMaxMissed = maxMissed;
    webSocketHeartbeatEnabled = true;
    
}

void WebsocketClient::disableWebSocketHeartbeat() {
    webSocketHeartbeatEnabled = false;
}

void WebsocketClient::enableWebSocketReconnect(unsigned long interval) {
    webSocketReconnectInterval = interval;
    webSocketReconnectEnabled = true;

}

void WebsocketClient::disableWebSocketReconnect() {
    webSocketReconnectEnabled = false;
}

void WebsocketClient::poll() {
    if (webSocketHeartbeatEnabled) {
        if (millis() - webSocketHeartbeatLastSent > webSocketHeartbeatInterval) {
            if (webSocketHeartbeatMissed >= webSocketHeartbeatMaxMissed) {
                close();
                return;
            }
            sendPing();
            webSocketHeartbeatLastSent = millis();
            webSocketHeartbeatMissed++;
        }
        if (millis() - webSocketHeartbeatLastSent > webSocketHeartbeatTimeout) {
            close();
            return;
        }
    }
    if (webSocketReconnectEnabled) {
        if (millis() - webSocketReconnectLastAttempt > webSocketReconnectInterval) {
            if (!connected()) {
                connect();
            }
            webSocketReconnectLastAttempt = millis();
        }
    }

    websockets::WebsocketsClient::poll();
}