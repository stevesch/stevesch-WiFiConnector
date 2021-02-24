#include "wifiConnector.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

namespace {
void mgrSetup();
void mgrConfig();
// void mgrLoop();

String sConfigPortalName;
String sConfigPortalPassword;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -8 * 3600;
const int   daylightOffset_sec = 3600;

std::function<void (bool)> indicateConfigActive = [](bool active){};

//////////////////////////////////////////////////////////////////////////////

bool sbUpdating = false;

void setUpdating(bool updating)
{
  if (updating) {
    SPIFFS.end();
    sbUpdating = true;
  } else {
    sbUpdating = false;
    SPIFFS.begin();
  }
}


void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("WiFiConnector: Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setClockTime()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}
//////////////////////////////////////////////////////////////////////////////

void handleConfig(AsyncWebServerRequest* request);

void wifiConnected();

DNSServer dnsServer;
AsyncWiFiManager* wifiManager = nullptr;

void otaSetup()
{
  if (!sConfigPortalName.isEmpty()) {
    ArduinoOTA.setHostname(sConfigPortalName.c_str());
  }

  // No authentication by default
  if (!sConfigPortalPassword.isEmpty()) {
    ArduinoOTA.setPassword(sConfigPortalPassword.c_str());
  }

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      }
      else { // U_SPIFFS
        type = "filesystem";
      }
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      setUpdating(true);
      Serial.println("WiFiConnector: Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      setUpdating(false);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      setUpdating(false);
      Serial.printf("Error[%u]: ", error);
      const char* msg = 0;
      switch (error) {
        case OTA_AUTH_ERROR: msg = "Auth Failed"; break;
        case OTA_BEGIN_ERROR: msg = "Begin Failed"; break;
        case OTA_CONNECT_ERROR: msg = "Connect Failed"; break;
        case OTA_RECEIVE_ERROR: msg = "Receive Failed"; break;
        case OTA_END_ERROR: msg = "End Failed"; break;
        default: msg = "Unknown"; break;
      }
      Serial.println(msg);
    });
}

void otaOnWifiConnect()
{
  ArduinoOTA.begin();
  
  // Add service to MDNS-SD
  // MDNS.addService("_http", "_tcp", 80);
  MDNS.addService("http", "tcp", 80);
}

void otaLoop()
{
  if (WiFi.isConnected()) {
    ArduinoOTA.handle();
  }
}

void configModeCallback(AsyncWiFiManager* mgr)
{
  Serial.println("WiFiConnector: Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(mgr->getConfigPortalSSID());
}

//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("WiFiConnector: should save config");
  shouldSaveConfig = true;

  if (WiFi.isConnected()) {
    wifiConnected();
  }
}

void mgrSetup()
{
  // WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  // wifiManager->addParameter(&custom_mqtt_server);

  wifiManager->setAPCallback(configModeCallback);
  wifiManager->setSaveConfigCallback(saveConfigCallback);

  mgrConfig();
}

void mgrConfig()
{
  if (!wifiManager) {
    Serial.println("WiFiConnector ERROR: call WiFiConnector::setup before calling WiFiConnector::config");
    return;
  }
  Serial.println("WiFiConnector: starting config...");
  indicateConfigActive(true);

  const char* portalName = sConfigPortalName.c_str();
  const char* portalAuth = !sConfigPortalPassword.isEmpty() ? sConfigPortalPassword.c_str() : nullptr;
  wifiManager->startConfigPortal(portalName, portalAuth);
}

// void mgrLoop()
// {
// }

bool lastStatusConnected = false;
void wifiClear()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    lastStatusConnected = WiFi.status() == WL_CONNECTED;
    delay(100);
}

void handleConfig(AsyncWebServerRequest* request) {
  String str = "WiFiConnector: handleConfig";
  Serial.println(str);
  mgrConfig();
  request->redirect("/");
}

void wifiConnected()
{
  Serial.println("WiFiConnector: WiFi connected.");
  otaOnWifiConnect();
  indicateConfigActive(false);

  setClockTime();
}

void formatDeviceId(String& nameOut)
{
  uint64_t chipid = ESP.getEfuseMac(); // i.e. MAC address, 6 bytes
  uint16_t chip = (uint16_t)(chipid>>32);
  char name[32];
  snprintf(name, 31, "esp32-%04X", chip);
  nameOut = name;
}
} // namespace

namespace stevesch {
namespace WiFiConnector {
void setup(AsyncWebServer* server,
  char const* configPortalName,
  char const* configPortalPassword)
{
  Serial.println("WiFiConnector: Starting WiFi...");
  // This is a little kludgy, but the AsyncWiFiManager constructor
  wifiManager = new AsyncWiFiManager(server, &dnsServer);

  wifiClear();

  if (configPortalPassword) {
    sConfigPortalPassword = configPortalPassword;
  }
  if (configPortalName) {
    sConfigPortalName = configPortalName;
  } else {
    formatDeviceId(sConfigPortalName);
  }

  WiFi.setHostname(sConfigPortalName.c_str());

  mgrSetup();

  // -- Set up required URL handlers on the web server.
  server->on("/config", HTTP_GET, handleConfig);

  otaSetup();

  Serial.println("WiFiConnector: WiFi Ready.");
}

void config()
{
  mgrConfig();
}

void loop() 
{
  // mgrLoop();
  otaLoop();
}

void setActivityIndicator(std::function<void (bool)> fn)
{
  indicateConfigActive = fn;
}

bool isUpdating() {
  return sbUpdating;
}

} // namespace WiFiConnector
} // namespace stevesch
