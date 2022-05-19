#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <uri/UriBraces.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TOTP.h>

WiFiClient client;

// Your hmacKey (10 bytes)
uint8_t hmacKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
TOTP totp = TOTP(hmacKey, 10);

void setup() {

  Serial.begin(4800);
  
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect("Safe");

  // start the NTP client
  timeClient.begin();
  timeClient.update();
  String code = String(totp.getCode(timeClient.getEpochTime()));
  
  Serial.print('#');
  Serial.println(code);
}

void loop(){}
