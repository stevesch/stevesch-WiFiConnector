#include "wifiConnector.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

static void mgrSetup();
static void mgrConfig();
static void mgrLoop();

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -8 * 3600;
const int   daylightOffset_sec = 3600;

std::function<void (bool)> indicateConfigActive = [](bool active){};

//////////////////////////////////////////////////////////////////////////////

// "isUpdating" == "an OTA update of code or files is occurring"
static bool sbUpdating = false;
bool isUpdating() {
  return sbUpdating;
}


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
    Serial.println("WiFi Connector: Failed to obtain time");
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
#ifdef ESP_NAME
  ArduinoOTA.setHostname(ESP_NAME);
#endif

#ifdef ESP_AUTH
  // No authentication by default
  ArduinoOTA.setPassword(ESP_AUTH);
#endif

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
      Serial.println("WiFi Connector: Start updating " + type);
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
  Serial.println("WiFi Connector: Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(mgr->getConfigPortalSSID());
}

//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
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

  // wifiManager->autoConnect(ESP_NAME, ESP_AUTH);
  wifiManager->startConfigPortal(ESP_NAME, ESP_AUTH);
}

void mgrConfig()
{
  if (!wifiManager) {
    Serial.println("WiFi Connector ERROR: call wifiSetup before calling wifiConfig");
    return;
  }
  Serial.println("WiFi Connector: starting config...");
  indicateConfigActive(true);
  wifiManager->startConfigPortal(ESP_NAME, ESP_AUTH);
}


void mgrLoop()
{
}

static bool lastStatusConnected = false;
void wifiClear()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    lastStatusConnected = WiFi.status() == WL_CONNECTED;
    delay(100);
}

void wifiSetActivityIndicator(std::function<void (bool)> fn)
{
  indicateConfigActive = fn;
}

void wifiSetup(AsyncWebServer* server) 
{
  Serial.println("WiFi Connector: Starting WiFi...");
  // This is a little kludgy, but the AsyncWiFiManager constructor
  wifiManager = new AsyncWiFiManager(server, &dnsServer);

  wifiClear();
#ifdef ESP_NAME
  WiFi.setHostname(ESP_NAME);
#endif

  mgrSetup();

  // -- Set up required URL handlers on the web server.
  server->on("/config", HTTP_GET, handleConfig);

  otaSetup();

  Serial.println("WiFi Connector: WiFi Ready.");
}

void wifiConfig()
{
  mgrConfig();
}

void wifiLoop() 
{
  mgrLoop();
  otaLoop();
}

const char* rowBegin() { return "<p class=aligned-row>"; }
const char* rowEnd() { return "</p>"; }

String createButton(String label, String link)
{
  String str = "<a class='button button-off' onclick=\"location.href='";
  str += link;
  str += "';\" >";
  str += label;
  str += "</a>";
  return str;

  // String str = "<a class=\"button button-off\" href=\"";
  // str += link;
  // str += "\">";
  // str += label;
  // str += "</a>";
  // return str;
}

void handleConfig(AsyncWebServerRequest* request) {
  String str = "WiFi Connector: handleConfig";
  Serial.println(str);
  mgrConfig();
  request->redirect("/");
}

void wifiConnected()
{
  Serial.println("WiFi Connector: WiFi connected.");
  otaOnWifiConnect();
  indicateConfigActive(false);

  setClockTime();
}
