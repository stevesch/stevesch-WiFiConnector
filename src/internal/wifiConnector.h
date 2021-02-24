#ifndef STEVESCH_WIFICONNECTOR_INTERNAL_WIFICONNECTOR_H_
#define STEVESCH_WIFICONNECTOR_INTERNAL_WIFICONNECTOR_H_
#include <functional>

class AsyncWebServer; // from <ESPAsyncWebServer.h>

namespace stevesch
{
namespace WifiConnector {
// callback is called when wifi config starts/stops-- call
// before wifiSetup to receive all notifications:
void setActivityIndicator(std::function<void (bool)> callback);

void setup(AsyncWebServer* server);
void loop();
void config();

bool isUpdating();

}
}

#endif
