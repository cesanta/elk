// Copyright (c) 2021 Cesanta Software Limited
// All rights reserved
//
// This Arduino sketch demonstrates "elk" JavaScript library integration:
//
//  1. Edit `ssid` and `pass` variables below
//  2. Flash this sketch to your ESP32 board
//  3. See board's IP address printed on a Serial Console
//  4. Visit http://elk-js.com, enter board's IP address, click "connect"

#include <WiFi.h>
#include "JS.h"

// const char *ssid = "WIFI_NETWORK";
// const char *pass = "WIFI_PASSWORD";
const char *ssid = "VMDF554B9";
const char *pass = "Mp7wjmamPafa";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) Serial.print("."), delay(300);
  Serial.print("\nConnected, IP address: ");
  Serial.println(WiFi.localIP());

  JS.begin();
}

void loop() {
}
