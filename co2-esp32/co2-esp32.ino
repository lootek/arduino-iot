#define MQTT_VERSION 3

#include <EspMQTTClient.h>
#include <DHT.h>
#include <Arduino.h>
#include <SensirionI2cScd30.h>
#include <Wire.h>
//#include "../wifi-creds.h"
#include "/mnt/data/projects/arduino-playground/wifi-creds.h"

const char* location = "dining_room";
const char* ssid = ssid_e;

SensirionI2cScd30 scd30;
static char errorMessage[128];
static int16_t error;

const uint8_t SCD30_CALIBRATION_PIN = 13;  // D13: HIGH enables forced calibration

EspMQTTClient client(
  ssid,
  password,
  "192.168.10.18",  // MQTT Broker server ip
  "",               // MQTTUsername, Can be omitted if not needed
  "",               // MQTTPassword, Can be omitted if not needed
  location,         // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

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

void consider_recalibrating_scd30(Stream&);
void setup_scd30(Stream& serial) {
  pinMode(SCD30_CALIBRATION_PIN, INPUT_PULLDOWN);

  Wire.begin();
  scd30.begin(Wire, SCD30_I2C_ADDR_61);

  scd30.stopPeriodicMeasurement();
  scd30.softReset();
  delay(5000);

  uint8_t major = 0;
  uint8_t minor = 0;
  error = scd30.readFirmwareVersion(major, minor);
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

  error = scd30.activateAutoCalibration(0);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute setAutomaticSelfCalibration(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
  } else {
    serial.println("Automatic self-calibration disabled");
  }

  error = scd30.startPeriodicMeasurement(0);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
    return;
  }

  delay(10000);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  dht.begin();
  setup_scd30(Serial);
  setup_mqtt();
}

void measure(Stream& serial) {
  serial.println("measure()");

  float co2Concentration = 0.0;
  float temperature = 0.0;
  float humidity = 0.0;

  // error = scd30.blockingReadMeasurementData(co2Concentration, temperature, humidity);
  error = scd30.readMeasurementData(co2Concentration, temperature, humidity);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute blockingReadMeasurementData(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
    client.publish("/sensors/" + String(location) + "/scd30/error", String(errorMessage));
  }

  serial.print("co2Concentration: ");
  serial.println(co2Concentration);
  client.publish("/sensors/" + String(location) + "/scd30/co2", String(co2Concentration));

  serial.print("temperature (SCD30): ");
  serial.println(temperature);
  client.publish("/sensors/" + String(location) + "/scd30/temperature", String(temperature));

  serial.print("humidity (SCD30): ");
  serial.println(humidity);
  client.publish("/sensors/" + String(location) + "/scd30/humidity", String(humidity));

  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("DHT22 temperature measurement error");
  } else {
    Serial.print("temperature (DHT22): ");
    Serial.println(t);
    client.publish("/sensors/" + String(location) + "/dht22/temperature", String(t));
  }

  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("DHT22 humidity measurement error");
  } else {
    Serial.print("humidity (DHT22):");
    Serial.println(h);
    client.publish("/sensors/" + String(location) + "/dht22/humidity", String(h));
  }

  consider_recalibrating_scd30(serial);

  serial.println();
}

const uint8_t recalibration_interval_threshold = 6;
uint8_t recalibration_interval_cnt = 0;
void consider_recalibrating_scd30(Stream& serial) {
  if (digitalRead(SCD30_CALIBRATION_PIN) == HIGH) {
    recalibration_interval_cnt++;
    if (recalibration_interval_cnt%recalibration_interval_threshold != 0) {
      return;
    }
    
    serial.println("Forced recalibration triggered (D13 HIGH)");
    client.publish("/sensors/" + String(location) + "/scd30/calibration", "force");

    error = scd30.setAltitudeCompensation(310);
    if (error != NO_ERROR) {
      serial.print("Error trying to execute setForcedRecalibrationFactor(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      serial.println(errorMessage);
    } else {
      serial.print("Altitude compensation set to ");

      uint16_t altitudeCompensation;
      scd30.getAltitudeCompensation(altitudeCompensation);
      serial.println(altitudeCompensation);
      client.publish("/sensors/" + String(location) + "/scd30/alt_compensation", String(altitudeCompensation));
    }

    error = scd30.forceRecalibration(425);
    if (error != NO_ERROR) {
      serial.print("Error trying to execute setForcedRecalibrationFactor(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      serial.println(errorMessage);
    } else {
      serial.print("Ref CO2 set to ");

      uint16_t co2RefConcentration;
      scd30.getForceRecalibrationStatus(co2RefConcentration);
      serial.println(co2RefConcentration);
      client.publish("/sensors/" + String(location) + "/scd30/ref_co2", String(co2RefConcentration));
    }
  }
}

void loop() {
  measure(Serial);

  // MQTT
  client.loop();

  delay(30 * 1000);
}
