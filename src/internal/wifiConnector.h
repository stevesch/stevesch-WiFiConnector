#ifndef STEVESCH_WIFICONNECTOR_INTERNAL_WIFICONNECTOR_H_
#define STEVESCH_WIFICONNECTOR_INTERNAL_WIFICONNECTOR_H_
#include <functional>

class AsyncWebServer; // from <ESPAsyncWebServer.h>
class Print;

namespace stevesch
{
namespace WiFiConnector {
// callback is called when wifi config starts/stops-- call
// before wifiSetup to receive all notifications:
void setActivityIndicator(std::function<void (bool)> callback);

// callback for wifi connection started/lost
void setOnConnected(std::function<void (bool)> fn);

void setOtaOnStart(std::function<void ()> fn);
void setOtaOnProgress(std::function<void (unsigned int, unsigned int)> fn);
void setOtaOnEnd(std::function<void ()> fn);

void enableModeless(bool modeless); // call before setup

void setup(AsyncWebServer* server,
  char const* configPortalName=nullptr,
  char const* configPortalPassword=nullptr);

void loop();
void config();

bool isUpdating(); // "isUpdating" == "an OTA update of code or files is occurring"

uint32_t waitForConnection(uint32_t timeoutMillis=3000, uint32_t timeSliceMillis=10);
void printStatus(Print& output); // log IP address, etc.
}
}

#endif
