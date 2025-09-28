#include "wiFiConnector.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWiFiManager.h>
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

std::function<void ()> otaOnStart = [](){};
std::function<void (unsigned int, unsigned int)> otaOnProgress = [](unsigned int progress, unsigned int total){};
std::function<void ()> otaOnEnd = [](){};


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

typedef AsyncWiFiManager wifimgr_t; // alanswx
// typedef ESPAsync_WiFiManager wifimgr_t; // khoih-prog

DNSServer dnsServer;
wifimgr_t* wiFiManager = nullptr;

void otaSetup()
{
  if (!sConfigPortalName.isEmpty()) {
    const char* hostname = sConfigPortalName.c_str();
    Serial.printf("WiFiConnector: OTA Hostname = %s\n", hostname);
    ArduinoOTA.setHostname(hostname);
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
      otaOnStart();
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      setUpdating(false);
      otaOnEnd();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      otaOnProgress(progress, total);
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
  Serial.println("WiFiConnector: OTA begin");
  ArduinoOTA.begin();
  
  // Add service to MDNS-SD
  // NOTE: addService will automatically prepend '_' if the service or proto does not
  // already begin with '_', but we specify it ourselves here for clarity, since
  // e.g. python calls would need to use the full name with underscore.
  MDNS.addService("_http", "_tcp", 80);
}

void otaOnWifiLost()
{
  Serial.println("WiFiConnector: OTA end");
  ArduinoOTA.end();
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
}


static void printWiFiStatus()
{
  Serial.printf("WiFiConnector: ");
  stevesch::WiFiConnector::printStatus(Serial);
}


void apChangeCallback(wifimgr_t* mgr)
{
  Serial.println("WiFiConnector: apChangeCallback");
  printWiFiStatus();
  Serial.printf("Soft AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("Config SSID: %s\n", mgr->getConfigPortalSSID().c_str());
}

bool callbackInConnectedState = false;
void checkWiFiConnection()
{
  bool connected = WiFi.isConnected();
  if (callbackInConnectedState != connected)
  {
    callbackInConnectedState = connected;
    Serial.printf("WiFiConnector: WiFi ");

    if (connected) {
      Serial.println("connected.");
      otaSetup();
      otaOnWifiConnect();
      setClockTime();
    } else {
      Serial.println("disconnected.");
      otaOnWifiLost();
    }

    onConnected(connected);
  }
}

void wiFiClear()
{
  Serial.println("WiFiConnector: wifiClear");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  checkWiFiConnection();
  delay(100);
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
  checkWiFiConnection();
}

// void handleConfig(AsyncWebServerRequest* request) {
//   Serial.println("WiFiConnector: handleConfig");
//   mgrConfig();
//   request->redirect("/");
// }

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

void printStatus(Print& output)
{
  String strIp = WiFi.localIP().toString();
  output.printf("SSID: %10s  IP: %12s  RSSI: %d  ch: %d  Tx: %d  S: %d\n",
    WiFi.SSID().c_str(),
    strIp.c_str(),
    WiFi.RSSI(), WiFi.channel(), (int)WiFi.getTxPower(),
    (int)WiFi.status()
  );
  if (isUpdating()) {
    output.println("OTA update is being performed");
  }
}

// loop w/delays and yields until wifi is connected
uint32_t waitForConnection(uint32_t timeoutMillis, uint32_t timeSliceMillis)
{
  // loop w/delays and yields until wifi is connected
  unsigned long t0 = millis();
  unsigned long elapsed = 0;
  const unsigned long kWiFiWaitTime = timeoutMillis;
  while (!WiFi.isConnected())
  {
    delay(timeSliceMillis);
    unsigned long t1 = millis();
    elapsed = t1 - t0;
    if (elapsed >= kWiFiWaitTime) {
      break;
    }
    yield();
  }
  return (uint32_t)elapsed;
}

void setup(AsyncWebServer* server, char const* configPortalName, char const* configPortalPassword)
{
  Serial.println("WiFiConnector: Starting WiFi...");

  wiFiClear();

  if (configPortalPassword) {
    sConfigPortalPassword = configPortalPassword;
  }
  if (configPortalName) {
    sConfigPortalName = configPortalName;
  } else {
    formatDeviceId(sConfigPortalName);
  }
  const char* hostName = sConfigPortalName.c_str();
  WiFi.setHostname(hostName);

  // This is a little kludgy, but the wifimgr_t constructor
  wiFiManager = new wifimgr_t(server, &dnsServer); // alanswx
  // wiFiManager = new wifimgr_t(server, &dnsServer, hostName); // khoih-prog

  mgrSetup();
  yield();

  // -- Set up required URL handlers on the web server.
  // server->on("/config", HTTP_GET, handleConfig);

  bool connected = WiFi.isConnected();
  Serial.printf("WiFiConnector: WiFi Ready (Connected: %s)\n",
    (connected ? "TRUE" : "FALSE"));
  checkWiFiConnection();
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

void setOtaOnStart(std::function<void ()> fn)
{
  otaOnStart = fn;
}

void setOtaOnProgress(std::function<void (unsigned int, unsigned int)> fn)
{
  otaOnProgress = fn;
}

void setOtaOnEnd(std::function<void ()> fn)
{
  otaOnEnd = fn;
}

bool isUpdating() {
  return sbUpdating;
}

stevesch::WiFiConnector::Status currentStatus()
{
  Status status;
  status.connected = WiFi.isConnected();
  if (status.connected) {
    status.ip = WiFi.localIP();
    status.ssid = WiFi.SSID();
    status.rssi = WiFi.RSSI();
    status.channel = WiFi.channel();
  }
  else
  {
    status.ip = IPAddress();
    status.ssid = "";
    status.rssi = 0;
    status.channel = 0;
  }
  return status;
}

void getCurrentStatus(Status &out)
{
  out = currentStatus();
}

} // namespace WiFiConnector
} // namespace stevesch
