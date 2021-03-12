#include <Arduino.h>
#include <pgmspace.h>   // for PROGMEM
#include <ESPAsyncWebServer.h>

#include <stevesch-WiFiConnector.h>

// some simple timing and log output
long lastLog = -1;
constexpr long kLogPeriod = 1000;
int logsPerformed = 0;
void printWiFiStaus();

AsyncWebServer server(80);

// defined below:
void handleWifiConnected(bool connected);
void handleIndex(AsyncWebServerRequest *request);
void handlePageNotFound(AsyncWebServerRequest* request);

int numConnectStart = 0;
int numConnectLost = 0;

void onWiFiConnect()
{
  ++numConnectStart;

  Serial.println("Example: starting server");
  server.on("/", HTTP_GET, handleIndex);
  server.onNotFound(handlePageNotFound);
  server.begin();
}

void onWiFiLost()
{
  ++numConnectLost;

  Serial.println("Example: Shutting down server");
  server.end();
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  // ESP_NAME and ESP_AUTH are defined in platformio.ini for this example
  stevesch::WiFiConnector::enableModeless(true);
  stevesch::WiFiConnector::setOnConnected(handleWifiConnected);
  stevesch::WiFiConnector::setup(&server, ESP_NAME, ESP_AUTH);
}

void loop()
{
  stevesch::WiFiConnector::loop();

  long tNow = millis();
  long delta = (tNow - lastLog);
  if (delta > kLogPeriod)
  {
    lastLog = tNow;
    ++logsPerformed;

    printWiFiStaus();
  }
}


void printWiFiStaus()
{
  String strIp = WiFi.localIP().toString();
  Serial.printf("Example: %10s %12s RSSI: %d  ch: %d  Tx: %d  +%d -%d =%d\n",
    WiFi.SSID().c_str(),
    strIp.c_str(),
    WiFi.RSSI(), WiFi.channel(), (int)WiFi.getTxPower(),
    numConnectStart, numConnectLost, (numConnectStart - numConnectLost)
  );
  if (stevesch::WiFiConnector::isUpdating()) {
    Serial.println("Example: OTA update is being performed");
  }
}


const char kSimplePage[] PROGMEM = R"delim(<!DOCTYPE HTML>
<html><head><title>WiFiConnector Test Page</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
html { font-family: Verdana; display: inline-block; text-align: center; }
body { margin: 0; }
p { font-size: 1.2rem; }
.titlebar { overflow: hidden; background-color: #29a64f; color: white; font-size: 0.6rem; }
</style></head><body>
<div class="titlebar"><h1>WiFiConnector Test</h1></div>
<div><p>SSID: %SSID_FIELD%</p></div>
<div><p>Name: %NAME_FIELD%</p></div>
<div><p>IP: %IP_FIELD%</p></div>
<div><p>Loaded at: %TIME%</p></div>
</body></html>)delim";


String getLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Failed to obtain time";
  }

  char timeStringBuff[64];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void handleWifiConnected(bool connected)
{
  Serial.printf("Example: WiFi connected: %s\n", connected ? "TRUE" : "FALSE");
  if (connected) {
    onWiFiConnect();
  } else {
    onWiFiLost();
  }
}

void handleIndex(AsyncWebServerRequest *request)
{
  String strSsid(WiFi.SSID());
  String strName(WiFi.getHostname());
  String strIp(WiFi.localIP().toString());
  String strTime(getLocalTime());
  String str(kSimplePage);
  // Note: these fields are replaced once (only upon initial page load):
  str.replace("%SSID_FIELD%", strSsid.c_str());
  str.replace("%NAME_FIELD%", strName.c_str());
  str.replace("%IP_FIELD%", strIp.c_str());
  str.replace("%TIME%", strTime.c_str());
  request->send(200, "text/html", str);
}

void handlePageNotFound(AsyncWebServerRequest* request) {
  Serial.println("Example: handlePageNotFound");
  request->redirect("/");
}
