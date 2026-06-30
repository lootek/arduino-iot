#define MQTT_VERSION 4

#include <EspMQTTClient.h>
#include <DHT.h>

#include "../wifi-creds.h"
#include "../dht22_pins_layout.h"

const char* location = "basement";
const char* ssid = ssid_h;

const bool debug = false;

EspMQTTClient client(
  ssid,
  password,
  "192.168.10.18",  // MQTT Broker server ip
  "",               // MQTTUsername, Can be omitted if not needed
  "",               // MQTTPassword, Can be omitted if not needed
  location,         // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

DHT dht(dht22_pin_for_location(location), DHT22);

String lastWillTopic;

void setup_mqtt() {
  client.enableDebuggingMessages(debug);
  client.enableMQTTPersistence();
  client.setKeepAlive(90);
  lastWillTopic = String("/sensors/") + location + String("/lastwill");
  client.enableLastWillMessage(lastWillTopic.c_str(), "I am going offline");
}

void onConnectionEstablished() {
  client.publish("/sensors/" + String(location) + "/mqtt/status", "ready");
  client.publish("/sensors/" + String(location) + "/wifi/ip", String(client.getMqttServerIp()));
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  setup_mqtt();
}

void measure(Stream& serial) {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("Temp measurement error");
  } else {
    Serial.println(t);
    client.publish("/sensors/" + String(location) + "/dht22/temperature", String(t));
  }

  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Humidity measurement error");
  } else {
    Serial.println(h);
    client.publish("/sensors/" + String(location) + "/dht22/humidity", String(h));
  }

  if (debug) {
    Serial.print("temperature = ");
    Serial.println(t);

    Serial.print("humidity = ");
    Serial.println(h);
  }
}

void loop() {
  measure(Serial);

  // MQTT
  client.loop();

  if (debug) {
    delay(5 * 1000);
  } else {
    delay(30 * 1000);
  }
}
