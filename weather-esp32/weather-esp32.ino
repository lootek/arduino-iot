#include <DHT_U.h>
#include <DHT.h>

#define DHT_SENSOR_PIN  4
#define DHT_SENSOR_TYPE DHT22

DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

void setup() {
  Serial.begin(9600);
  dht_sensor.begin();
}

void loop() {
  float humidity  = dht_sensor.readHumidity();
  float temperature_C = dht_sensor.readTemperature();

  if ( isnan(temperature_C) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print(humidity);
    Serial.print(temperature_C);
  }

  // wait a 2 seconds between readings
  delay(2000);
}
