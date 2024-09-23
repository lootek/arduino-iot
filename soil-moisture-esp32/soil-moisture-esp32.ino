#include <EspMQTTClient.h>
#include <driver/uart.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "/mnt/data/projects/arduino-playground/wifi-creds.h"

const bool debug = false;

EspMQTTClient client(
  ssid,
  password,
  "192.168.10.18",      // MQTT Broker server ip
  "",                   // MQTTUsername, Can be omitted if not needed
  "",                   // MQTTPassword, Can be omitted if not needed
  "soil_1",             // Client name that uniquely identify your device
  1883                  // The MQTT port, default to 1883. this line can be omitted
);

#define DHTPIN 22
#define DHTTYPE    DHT11
DHT dht(DHTPIN, DHTTYPE);

String readDHTTemperature() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("Temp measurement error");
    return "--";
  } else {
    Serial.println(t);
    return String(t);
  }
}

String readDHTHumidity() {
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Humidity measurement error");
    return "--";
  } else {
    Serial.println(h);
    return String(h);
  }
}

void setup_mqtt()
{
  client.enableDebuggingMessages(debug);
  client.enableMQTTPersistence();
  client.setKeepAlive(90);
  client.enableLastWillMessage("/sensors/soil_1/lastwill", "I am going offline");
}

void onConnectionEstablished()
{
  client.publish("/sensors/soil_1/mqtt/status", "ready");
  client.publish("/sensors/soil_1/wifi/ip", String(client.getMqttServerIp()));
}

void setup()
{
  Serial.begin(115200);

  setup_mqtt();
}

void measure(Stream& serial) {
  float t = dht.readTemperature();
  if (!isnan(t)) {
    client.publish("/sensors/soil_1/dht11/temperature", String(t));
  }

  float h = dht.readHumidity();
  if (!isnan(h)) {
    client.publish("/sensors/soil_1/dht11/humidity", String(h));
  }
  
  if (debug) {
    Serial.print("temperature = ");
    Serial.println(t);

    Serial.print("humidity = ");
    Serial.println(h);
  }
}

void loop()
{
  measure(Serial);

  // MQTT
  client.loop();

  if (debug) {
    delay(5 * 1000);
  } else {
    delay(30 * 1000);
  }
}
