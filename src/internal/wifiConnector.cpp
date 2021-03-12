#include "wiFiConnector.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
// #include <ESPAsyncWiFiManager.h>  // alanswx/ESPAsyncWiFiManager@^0.23
#include <ESPAsync_WiFiManager.h> // https://github.com/khoih-prog/ESPAsync_WiFiManager
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

namespace {
void mgrSetup();
void mgrConfig();
void mgrLoop();

String sConfigPortalName;
String sConfigPortalPassword;
bool sModeless = false;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -8 * 3600;
const int   daylightOffset_sec = 3600;

std::function<void (bool)> indicateConfigActive = [](bool active){};
std::function<void (bool)> onConnected = [](bool connected){};

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

// void handleConfig(AsyncWebServerRequest* request);

void wiFiConnected();

// typedef AsyncWiFiManager wifimgr_t;
typedef ESPAsync_WiFiManager wifimgr_t;

DNSServer dnsServer;
wifimgr_t* wiFiManager = nullptr;

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
  // NOTE: addService will automatically prepend '_' if the service or proto does not
  // already begin with '_', but we specify it ourselves here for clarity, since
  // e.g. python calls would need to use the full name with underscore.
  MDNS.addService("_http", "_tcp", 80);
}

void otaLoop()
{
  if (WiFi.isConnected()) {
    ArduinoOTA.handle();
  }
}

//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("WiFiConnector: should save config");
  shouldSaveConfig = true;
  indicateConfigActive(false);

  if (WiFi.isConnected()) {
    wiFiConnected();
  }
}


void printWiFiStaus()
{
  String strIp = WiFi.localIP().toString();
  Serial.printf("WiFiConnector: %10s %12s RSSI: %d  ch: %d  Tx: %d\n",
    WiFi.SSID().c_str(),
    strIp.c_str(),
    WiFi.RSSI(), WiFi.channel(), (int)WiFi.getTxPower()
  );
  if (stevesch::WiFiConnector::isUpdating()) {
    Serial.println("WiFiConnector: OTA update is being performed");
  }
}


void apChangeCallback(wifimgr_t* mgr)
{
  Serial.println("WiFiConnector: apChangeCallback");
  printWiFiStaus();
  Serial.printf("Soft AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("Config SSID: %s\n", mgr->getConfigPortalSSID().c_str());
}

void mgrSetup()
{
  // WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  // wiFiManager->addParameter(&custom_mqtt_server);

  wiFiManager->setAPCallback(apChangeCallback);
  wiFiManager->setSaveConfigCallback(saveConfigCallback);

  mgrConfig();
}

void mgrConfig()
{
  if (!wiFiManager) {
    Serial.println("WiFiConnector ERROR: call WiFiConnector::setup before calling WiFiConnector::config");
    return;
  }
  Serial.println("WiFiConnector: starting config...");
  indicateConfigActive(true);

  const char* portalName = sConfigPortalName.c_str();
  const char* portalAuth = !sConfigPortalPassword.isEmpty() ? sConfigPortalPassword.c_str() : nullptr;
  if (sModeless) {
    wiFiManager->startConfigPortalModeless(portalName, portalAuth);
  } else {
    wiFiManager->startConfigPortal(portalName, portalAuth);
  }
}

void mgrLoop()
{
  // only for startConfigPortalModeless? causes log spam when startConfigPortal is used
  if (sModeless) {
    wiFiManager->loop();
  }
}

bool lastStatusConnected = false;

void wiFiClear()
{
  Serial.println("WiFiConnector: wifiClear");
  if (WiFi.isConnected()) {
    onConnected(false);
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  lastStatusConnected = WiFi.status() == WL_CONNECTED;
  delay(100);
}

// void handleConfig(AsyncWebServerRequest* request) {
//   Serial.println("WiFiConnector: handleConfig");
//   mgrConfig();
//   request->redirect("/");
// }

void wiFiConnected()
{
  Serial.println("WiFiConnector: WiFi connected.");
  otaSetup();
  otaOnWifiConnect();

  setClockTime();
  onConnected(true);
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

void enableModeless(bool modeless)
{
  sModeless = modeless;
}


void setup(AsyncWebServer* server, char const* configPortalName, char const* configPortalPassword)
{
  Serial.println("WiFiConnector: Starting WiFi...");

  if (configPortalPassword) {
    sConfigPortalPassword = configPortalPassword;
  }
  if (configPortalName) {
    sConfigPortalName = configPortalName;
  } else {
    formatDeviceId(sConfigPortalName);
  }
  WiFi.setHostname(sConfigPortalName.c_str());

  // This is a little kludgy, but the wifimgr_t constructor
  wiFiManager = new wifimgr_t(server, &dnsServer);
  // (&webServer, &dnsServer, "ModelessConnect");

  wiFiClear();

  mgrSetup();

  // -- Set up required URL handlers on the web server.
  // server->on("/config", HTTP_GET, handleConfig);

  Serial.printf("WiFiConnector: WiFi Ready (Connected: %s)\n",
    (WiFi.isConnected() ? "TRUE" : "FALSE"));
  if (WiFi.isConnected()) {
    onConnected(true);
  }
}

void config()
{
  mgrConfig();
}

void loop() 
{
  mgrLoop();
  otaLoop();
}

void setActivityIndicator(std::function<void (bool)> fn)
{
  indicateConfigActive = fn;
}

void setOnConnected(std::function<void (bool)> fn)
{
  onConnected = fn;
}

bool isUpdating() {
  return sbUpdating;
}

} // namespace WiFiConnector
} // namespace stevesch
