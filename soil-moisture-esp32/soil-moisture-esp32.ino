#include <EspMQTTClient.h>
#include <driver/uart.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "/mnt/data/projects/arduino-playground/wifi-creds.h"

const char* location = "soil_2";
const char* ssid = ssid_e;
const int deep_sleep_duration = 3600; // 1h

const bool debug = true;

EspMQTTClient client(
  ssid,
  password,
  "192.168.10.18",      // MQTT Broker server ip
  "",                   // MQTTUsername, Can be omitted if not needed
  "",                   // MQTTPassword, Can be omitted if not needed
  location,             // Client name that uniquely identify your device
  1883                  // The MQTT port, default to 1883. this line can be omitted
);

DHT dht(22, DHT11);

void setup_mqtt()
{
  client.enableDebuggingMessages(debug);
  client.enableMQTTPersistence();
  client.setKeepAlive(90);
  client.enableLastWillMessage((String("/sensors/") + location + String("/lastwill")).c_str(), (location + String(" going offline")).c_str());
}

void setup_deep_sleep()
{
  if (deep_sleep_duration <= 0) {
    return;
  }
  
  esp_sleep_enable_timer_wakeup(deep_sleep_duration * 1000000ULL); // [us] -> [s]
  Serial.println("Will wake up every " + String(deep_sleep_duration) + "s");
}

void onConnectionEstablished()
{
  client.publish("/sensors/" + String(location) + "/mqtt/status", "ready");
  client.publish("/sensors/" + String(location) + "/wifi/ip", String(client.getMqttServerIp()));
}

void setup()
{
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  dht.begin();
  setup_mqtt();
  setup_deep_sleep();
}

void measure(Stream& serial) {
  int soil_h = 4096 - analogRead(32);
  if (isnan(soil_h)) {
    Serial.println("Soil humidity measurement error");
  } else {
    Serial.println(soil_h);
    client.publish("/sensors/" + String(location) + "/analog/humidity", String(soil_h));
  }

  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("Temp measurement error");
  } else {
    Serial.println(t);
    client.publish("/sensors/" + String(location) + "/dht11/temperature", String(t));
  }

  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Humidity measurement error");
  } else {
    Serial.println(h);
    client.publish("/sensors/" + String(location) + "/dht11/humidity", String(h));
  }
}

void loop()
{
  client.loop();
  measure(Serial);
  delay(2 * 1000);
  Serial.println("deep sleep duration: " + String(deep_sleep_duration) + ", WiFi: " + String(client.isWifiConnected()) + ", MQTT: " + String(client.isMqttConnected()));

  if (deep_sleep_duration > 0 && client.isConnected()) {
    Serial.println("Going to deep sleep now");
    Serial.flush();
    esp_deep_sleep_start();
  }
}
