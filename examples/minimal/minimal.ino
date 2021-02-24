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

void handleIndex(AsyncWebServerRequest *request);         // defined below
void handlePageNotFound(AsyncWebServerRequest* request);  // defined below

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  // ESP_NAME and ESP_AUTH are defined in platformio.ini for this example
  stevesch::WiFiConnector::setup(&server, ESP_NAME, ESP_AUTH);

  server.on("/", HTTP_GET, handleIndex);
  server.onNotFound(handlePageNotFound);
  server.begin();
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
  Serial.printf("%10s %12s RSSI: %d  ch: %d  Tx: %d\n",
    WiFi.SSID().c_str(),
    strIp.c_str(),
    WiFi.RSSI(), WiFi.channel(), (int)WiFi.getTxPower()
  );
  if (stevesch::WiFiConnector::isUpdating()) {
    Serial.println("OTA update is being performed");
  }
}


const char kSimplePage[] PROGMEM = R"delim(
<!DOCTYPE HTML>
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
</body></html>)delim";

void handleIndex(AsyncWebServerRequest *request)
{
  String strSsid(WiFi.SSID());
  String strName(WiFi.getHostname());
  String strIp(WiFi.localIP().toString());
  String str(kSimplePage);
  // Note: these fields are replaced once (only upon initial page load):
  str.replace("%SSID_FIELD%", strSsid.c_str());
  str.replace("%NAME_FIELD%", strName.c_str());
  str.replace("%IP_FIELD%", strIp.c_str());
  request->send(200, "text/html", str);
}

void handlePageNotFound(AsyncWebServerRequest* request) {
  Serial.println("Callback: handlePageNotFound");
  request->redirect("/");
}
