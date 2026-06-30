#define MQTT_VERSION 4

#include <EspMQTTClient.h>
#include <DHT.h>
#include <Arduino.h>
#include <SensirionI2cScd30.h>
#include <Wire.h>
#include "../wifi-creds.h"
#include "../dht22_pins_layout.h"
#include "../wifi_diagnostics.h"

const char* location = "dining_room";
const char* ssid = ssid_e;

SensirionI2cScd30 scd30;
static char errorMessage[128];
static int16_t error;

// FRC fires once per boot after this many valid readings (~10 min at 30s loop).
// Long enough for the NDIR chamber to equilibrate to ambient outside air after
// the sensor has been inside (elevated CO2) for an extended period.
const uint16_t FRC_WARMUP_READINGS = 20;
uint16_t valid_reading_count = 0;
bool frc_done = false;

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
  client.enableDebuggingMessages(true);
  client.enableMQTTPersistence();
  client.setKeepAlive(90);
  lastWillTopic = String("/sensors/") + location + String("/lastwill");
  client.enableLastWillMessage(lastWillTopic.c_str(), "I am going offline");
}

void onConnectionEstablished() {
  client.publish("/sensors/" + String(location) + "/mqtt/status", "ready");
  publish_connected_wifi_info(client, location);
}

void consider_recalibrating_scd30(Stream&);
void setup_scd30(Stream& serial) {
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

  error = scd30.setMeasurementInterval(2);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute setMeasurementInterval(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
  } else {
    serial.println("Measurement interval set to 2s");
  }

  error = scd30.startPeriodicMeasurement(0);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
    return;
  }
  serial.println("Periodic measurement started");

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

  uint16_t dataReady = 0;
  error = scd30.getDataReady(dataReady);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute getDataReady(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
    client.publish("/sensors/" + String(location) + "/scd30/error", String(errorMessage));
  } else {
    serial.print("dataReady: ");
    serial.println(dataReady);
    client.publish("/sensors/" + String(location) + "/scd30/data_ready", String(dataReady));
  }

  if (dataReady == 0) {
    serial.println("SCD30 not producing data (data-ready flag false)");
    client.publish("/sensors/" + String(location) + "/scd30/status", "no_data");
  } else {
    error = scd30.readMeasurementData(co2Concentration, temperature, humidity);
    if (error != NO_ERROR) {
      serial.print("Error trying to execute readMeasurementData(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      serial.println(errorMessage);
      client.publish("/sensors/" + String(location) + "/scd30/error", String(errorMessage));
    } else if (!isnan(co2Concentration) && co2Concentration > 0) {
      valid_reading_count++;
      serial.print("co2Concentration: ");
      serial.println(co2Concentration);
      client.publish("/sensors/" + String(location) + "/scd30/co2", String(co2Concentration));

      serial.print("temperature (SCD30): ");
      serial.println(temperature);
      client.publish("/sensors/" + String(location) + "/scd30/temperature", String(temperature));

      serial.print("humidity (SCD30): ");
      serial.println(humidity);
      client.publish("/sensors/" + String(location) + "/scd30/humidity", String(humidity));
    } else {
      serial.print("SCD30 returned invalid reading: co2=");
      serial.println(co2Concentration);
      client.publish("/sensors/" + String(location) + "/scd30/status", "invalid_reading");
    }
  }

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

void consider_recalibrating_scd30(Stream& serial) {
  if (frc_done) {
    return;
  }
  if (valid_reading_count < FRC_WARMUP_READINGS) {
    return;
  }

  frc_done = true;
  serial.println("Forced recalibration triggered (warmup complete)");
  client.publish("/sensors/" + String(location) + "/scd30/calibration", "force");

  error = scd30.setAltitudeCompensation(310);
  if (error != NO_ERROR) {
    serial.print("Error setAltitudeCompensation(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
  } else {
    uint16_t altitudeCompensation;
    scd30.getAltitudeCompensation(altitudeCompensation);
    serial.print("Altitude compensation set to ");
    serial.println(altitudeCompensation);
    client.publish("/sensors/" + String(location) + "/scd30/alt_compensation", String(altitudeCompensation));
  }

  error = scd30.forceRecalibration(450);
  if (error != NO_ERROR) {
    serial.print("Error forceRecalibration(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
    client.publish("/sensors/" + String(location) + "/scd30/error", String(errorMessage));
  } else {
    uint16_t co2RefConcentration;
    scd30.getForceRecalibrationStatus(co2RefConcentration);
    serial.print("Ref CO2 set to ");
    serial.println(co2RefConcentration);
    client.publish("/sensors/" + String(location) + "/scd30/ref_co2", String(co2RefConcentration));
  }
}

void loop() {
  measure(Serial);

  // MQTT
  client.loop();

  static uint16_t wifi_scan_tick = 0;
  if (++wifi_scan_tick >= 60) {
    wifi_scan_tick = 0;
    publish_wifi_scan(client, location);
  }

  delay(30 * 1000);
}
