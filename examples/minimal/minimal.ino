#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include <stevesch-WiFiConnector.h>

// some simple timing and log output
long lastLog = -1;
constexpr long kLogPeriod = 1000;
int logsPerformed = 0;
void printWiFiStaus();

AsyncWebServer server(80);

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  stevesch::WiFiConnector::setup(&server);
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
