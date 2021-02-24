#ifndef STEVESCH_WIFICONNECTOR_INTERNAL_WIFICONNECTOR_H_
#define STEVESCH_WIFICONNECTOR_INTERNAL_WIFICONNECTOR_H_
#include <functional>

class AsyncWebServer; // from <ESPAsyncWebServer.h>

// callback is called when wifi config starts/stops-- call
// before wifiSetup to receive all notifications:
void wifiSetActivityIndicator(std::function<void (bool)> callback);

void wifiSetup(AsyncWebServer* server);
void wifiLoop();
void wifiConfig();

bool isUpdating();

#endif
