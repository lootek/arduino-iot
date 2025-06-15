#define MQTT_VERSION 3

#include <EspMQTTClient.h>
#include <Arduino.h>
#include <SensirionI2cScd30.h>
#include <Wire.h>
#include "../wifi-creds.h"

const char* location = "dining_room";
const char* ssid = ssid_e;

SensirionI2cScd30 sensor;
static char errorMessage[128];
static int16_t error;

const bool debug = true;

EspMQTTClient client(
  ssid,
  password,
  "192.168.10.18",  // MQTT Broker server ip
  "",               // MQTTUsername, Can be omitted if not needed
  "",               // MQTTPassword, Can be omitted if not needed
  location,         // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

void setup_mqtt() {
  client.enableDebuggingMessages(debug);
  client.enableMQTTPersistence();
  client.setKeepAlive(90);
  client.enableLastWillMessage((String("/sensors/") + location + String("/lastwill")).c_str(), "I am going offline");
}

void onConnectionEstablished() {
  client.publish("/sensors/" + String(location) + "/mqtt/status", "ready");
  client.publish("/sensors/" + String(location) + "/wifi/ip", String(client.getMqttServerIp()));
}

void setup_scd30(Stream& serial) {
    Wire.begin();
    sensor.begin(Wire, SCD30_I2C_ADDR_61);

    sensor.stopPeriodicMeasurement();
    sensor.softReset();
    delay(2000);
    uint8_t major = 0;
    uint8_t minor = 0;
    error = sensor.readFirmwareVersion(major, minor);
    if (error != NO_ERROR) {
        serial.print("Error trying to execute readFirmwareVersion(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        serial.println(errorMessage);
        return;
    }
    serial.print("firmware version major: ");
    serial.print(major);
    serial.print("\t");
    serial.print("minor: ");
    serial.print(minor);
    serial.println();
    error = sensor.startPeriodicMeasurement(0);
    if (error != NO_ERROR) {
        serial.print("Error trying to execute startPeriodicMeasurement(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        serial.println(errorMessage);
        return;
    }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  setup_scd30(Serial);
  setup_mqtt();
}

void measure(Stream& serial) {
  float co2Concentration = 0.0;
  float temperature = 0.0;
  float humidity = 0.0;

  error = sensor.blockingReadMeasurementData(co2Concentration, temperature, humidity);
  if (error != NO_ERROR) {
      serial.print("Error trying to execute blockingReadMeasurementData(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      serial.println(errorMessage);
      client.publish("/sensors/" + String(location) + "/scd30/error", String(errorMessage));
      return;
  }

  serial.print("co2Concentration: ");
  serial.println(co2Concentration);
  client.publish("/sensors/" + String(location) + "/scd30/co2", String(co2Concentration));

  serial.print("temperature: ");
  serial.println(temperature);
  client.publish("/sensors/" + String(location) + "/scd30/temperature", String(temperature));

  serial.print("humidity: ");
  serial.println(humidity);
  client.publish("/sensors/" + String(location) + "/scd30/humidity", String(humidity));

  serial.println();
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
