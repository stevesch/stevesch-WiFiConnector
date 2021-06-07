# stevesch-WiFiConnector

Provides simplified setup for
- WiFi Manager (allow you to specify your WiFi LAN without placing SSID/password in code)
- over-the-air update (allow you to upload new code to your device via WiFi instead of USB)
- MDNS (allow you to acceess your device by name, e.g. http://mydevice.local, rather than IP)

Your sketch basically just needs to call setup and loop for the library.

examples/minimal provides a simple sketch that loops and outputs status (current SSID, IP, etc.) on the serial port.

code additions for minimal setup:
```
#include <ESPAsyncWebServer.h>
#include <stevesch-WiFiConnector.h>
AsyncWebServer server(80);

// connect to "mydevice" with, e.g., your phone to configure
// the WiFi network (LAN) that the ESP32 should use.  The following SSID
// and password are used only for initial configuration of the ESP32:
const char* ESP_NAME = "mydevice"; // SSID to broadcast to configure WiFi
const char* ESP_AUTH = "00000000"; // auth must be at least 8 characters

// once WiFi has been configured via, e.g. your phone, the device will
// be available on your WiFi LAN via "http://mydevice.local" (and directly
// via the IP addressed assigned to it, such as "http://192.168.xxx.xxx").

void setup()
{
  // (your other setup code here)

  stevesch::WiFiConnector::enableModeless(true);
  // Add the following handler to serve a web page (see example for details):
  // stevesch::WiFiConnector::setOnConnected(handleWifiConnected);
  stevesch::WiFiConnector::setup(&server, ESP_NAME, ESP_AUTH);
}

void loop()
{
  stevesch::WiFiConnector::loop();
  // (your other loop code here)
}
```

# Building and Running

To get up-and-running:
- PlatformIO is recommended
- Build this library (builds its own minimal example)
- Upload to any ESP32 board via USB
- When your board is booted, a WiFi config portal named "wificonnector-test" should be available.
- Connecting to the "wificonnector-test" WiFi network with any device (default password, specified in the platform.ini file, is 00000000) and set the config options for the WiFi network of your router (choose your network/SSID and enter its password).
- Connect to your router's WiFi network and open the ESP32 page in any browser.  The IP of the device will be displayed in the serial output if you're monitoring serial output of the board.  If your router supports mDNS, you should be able to open

"http://wificonnector-test.local/"

(otherwise you'll need to open, e.g.

"http://192.168.x.x" or "http://10.x.x.x"

where x.x/x.x.x matches the address displayed in the serial output from your board).

The following page should be displayed:

![Example Screencap](examples/minimal/example-minimal-screencap.jpg)

Building from this example, you can replace the HTML with your own page and page handling code.

## Over-the-air updating

Once you are able to properly connect and view your web page, over-the-air updates can be performed using the platformio.ini build configurations ending in "-OTA" (these are preconfigured to use the host name and optional password of "0000" specified in the platformio.ini file).
Choose "Upload" to upload code changes, and "Upload Filesystem Image" to upload files in your data folder to the SPIFFS file system (if you have added such files to the sketch).


![OTA Upload Screencap](examples/minimal/example-ota-upload.jpg)


If you receive this error:

[ERROR]: Host wificonnector-test.local Not Found

Your router is not routing mDNS information.  In this case, you can perform OTA updating by replacing

upload_port = ${ota.esp_name}.local

with your device's IP address, for example:

upload_port = 192.168.x.x

(where x.x are the digits from the IP of your device-- again, visible from the serial monitor if you attach USB temporarily and monitor the output).

