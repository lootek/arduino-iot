#define MQTT_VERSION 3

#include <EspMQTTClient.h>
#include <Wire.h>
#include <sps30.h>
#include <DHT.h>
#include "/mnt/data/projects/arduino-playground/wifi-creds.h"

const char* location = "patio";
const char* ssid = ssid_e;

EspMQTTClient client(
  ssid,
  password,
  "192.168.10.18",  // MQTT Broker server ip
  "",               // MQTTUsername, Can be omitted if not needed
  "",               // MQTTPassword, Can be omitted if not needed
  location,         // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);
  
int16_t ret;
uint8_t auto_clean_days = 4;
uint32_t auto_clean;
struct sps30_measurement m;
char serial[SPS30_MAX_SERIAL_LEN];
uint16_t data_ready;
DHT dht(4, DHT22);

void setup_mqtt() {
  client.enableDebuggingMessages(true);
  client.enableMQTTPersistence();
  client.setKeepAlive(90);
  client.enableLastWillMessage((String("/sensors/") + location + String("/lastwill")).c_str(), "I am going offline");
}

void onConnectionEstablished() {
  client.publish("/sensors/" + String(location) + "/mqtt/status", "ready");
  client.publish("/sensors/" + String(location) + "/wifi/ip", String(client.getMqttServerIp()));
}

void setup_sps() {
  delay(5000);

  Wire.begin(33,32);

  sensirion_i2c_init();
  while (sps30_probe() != 0) {
    Serial.print("SPS sensor probing failed\n");
    delay(500);
  }

  // Used to drive the fan for pre-defined sequence every X days
  ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
  if (ret) {
    Serial.print("error setting the auto-clean interval: ");
    Serial.println(ret);
  }

  delay(1000);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  setup_sps();
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
  
  sps30_start_measurement(); // Start of loop start fan to flow air past laser sensor
  delay(5000);

  while (true) {
    ret = sps30_read_data_ready(&data_ready); // Reads the last data from the sensor
    if (ret < 0) {
      serial.print("error reading data-ready flag: ");
      serial.println(ret);
    } else if (!data_ready)
      serial.print("data not ready, no new measurement available\n");
    else
      break;

    delay(250); // retry delay
  }

  ret = sps30_read_measurement(&m); // Ask SPS30 for measurments over I2C, returns 10 sets of data

  serial.print("PM  1.0: ");
  serial.println(m.mc_1p0);
  client.publish("/sensors/" + String(location) + "/sps30/mc_1p0", String(m.mc_1p0));
  serial.print("PM  2.5: ");
  serial.println(m.mc_2p5);
  client.publish("/sensors/" + String(location) + "/sps30/mc_2p5", String(m.mc_2p5));
  serial.print("PM  4.0: ");
  serial.println(m.mc_4p0);
  client.publish("/sensors/" + String(location) + "/sps30/mc_4p0", String(m.mc_4p0));
  serial.print("PM 10.0: ");
  serial.println(m.mc_10p0);
  client.publish("/sensors/" + String(location) + "/sps30/mc_10p0", String(m.mc_10p0));
  serial.print("NC  0.5: ");
  serial.println(m.nc_0p5);
  client.publish("/sensors/" + String(location) + "/sps30/nc_0p5", String(m.nc_0p5));
  serial.print("NC  1.0: ");
  serial.println(m.nc_1p0);
  client.publish("/sensors/" + String(location) + "/sps30/nc_1p0", String(m.nc_1p0));
  serial.print("NC  2.5: ");
  serial.println(m.nc_2p5);
  client.publish("/sensors/" + String(location) + "/sps30/nc_2p5", String(m.nc_2p5));
  serial.print("NC  4.0: ");
  serial.println(m.nc_4p0);
  client.publish("/sensors/" + String(location) + "/sps30/nc_4p0", String(m.nc_4p0));
  serial.print("NC 10.0: ");
  serial.println(m.nc_10p0);
  client.publish("/sensors/" + String(location) + "/sps30/nc_10p0", String(m.nc_10p0));
  serial.print("Typical partical size: ");
  serial.println(m.typical_particle_size);
  client.publish("/sensors/" + String(location) + "/sps30/typical_particle_size", String(m.typical_particle_size));
  serial.println();

  sps30_stop_measurement();
  delay(25 * 1000);
}

void loop() {
  measure(Serial);

  // MQTT
  client.loop();
}
