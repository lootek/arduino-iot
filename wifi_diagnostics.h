// wifi_diagnostics.h
// Publishes per-network WiFi diagnostics under BSSID-keyed MQTT topics:
//   /sensors/<location>/wifi/<bssid>/rssi      — for every visible network
//   /sensors/<location>/wifi/<bssid>/channel  — only for the connected AP
//   /sensors/<location>/wifi/<bssid>/ip        — only for the connected AP
//
// Split into two calls so the cheap connected-AP info can be published on
// every (re)connect without risking the radio brown-out that an active
// scan can trigger on marginal USB power: publish_connected_wifi_info()
// only reads WiFi state the STA already has; publish_wifi_scan() does the
// ~1–2 s synchronous active scan across channels and is best called rarely
// from loop() (every Nth iteration), not on the connect callback.

#pragma once

#include <EspMQTTClient.h>
#include <WiFi.h>

inline void publish_connected_wifi_info(EspMQTTClient& client, const char* location) {
  String connected_bssid = WiFi.BSSIDstr();
  String prefix = "/sensors/" + String(location) + "/wifi/" + connected_bssid + "/";
  client.publish(prefix + "rssi", String(WiFi.RSSI()));
  client.publish(prefix + "channel", String(WiFi.channel()));
  client.publish(prefix + "ip", WiFi.localIP().toString());
}

inline void publish_wifi_scan(EspMQTTClient& client, const char* location) {
  String connected_bssid = WiFi.BSSIDstr();
  int n = WiFi.scanNetworks(false, false);
  for (int i = 0; i < n; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    if (bssid == connected_bssid) {
      continue;
    }
    client.publish("/sensors/" + String(location) + "/wifi/" + bssid + "/rssi",
                   String(WiFi.RSSI(i)));
  }
  WiFi.scanDelete();
}