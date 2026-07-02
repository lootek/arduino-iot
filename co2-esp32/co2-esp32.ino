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

// SCD30 FRC calibration curve is stored in the sensor's non-volatile memory
// and persists across power cycles. We do NOT auto-FRC on boot - that would
// re-calibrate to whatever air the device is in at boot time (e.g. indoor
// 600-1500 ppm at home), destroying the outside-air calibration. To re-FRC,
// publish a ppm value to /sensors/<location>/scd30/cmd/frc (see onConnectionEstablished).
const uint16_t SCD30_ALTITUDE_COMPENSATION_M = 310;

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

  // On-demand FRC: publish a ppm value (400-2000) to this topic to force
  // recalibration against that reference. Only do this in a known reference
  // atmosphere (e.g. outside air ~430 ppm). The new calibration persists in
  // the SCD30's NVM across power cycles.
  client.subscribe("/sensors/" + String(location) + "/scd30/cmd/frc", [](const String& payload) {
    long ref = payload.toInt();
    if (ref < 400 || ref > 2000) {
      client.publish("/sensors/" + String(location) + "/scd30/error", "frc ref out of range: " + payload);
      return;
    }
    client.publish("/sensors/" + String(location) + "/scd30/calibration", "force:" + payload);

    error = scd30.setAltitudeCompensation(SCD30_ALTITUDE_COMPENSATION_M);
    if (error != NO_ERROR) {
      errorToString(error, errorMessage, sizeof errorMessage);
      client.publish("/sensors/" + String(location) + "/scd30/error", String("setAltitudeCompensation: ") + errorMessage);
    }

    error = scd30.forceRecalibration((uint16_t)ref);
    if (error != NO_ERROR) {
      errorToString(error, errorMessage, sizeof errorMessage);
      client.publish("/sensors/" + String(location) + "/scd30/error", String("forceRecalibration: ") + errorMessage);
    } else {
      uint16_t refBack = 0;
      scd30.getForceRecalibrationStatus(refBack);
      client.publish("/sensors/" + String(location) + "/scd30/ref_co2", String(refBack));
    }
  });
}

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

  error = scd30.setMeasurementInterval(15);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute setMeasurementInterval(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
  } else {
    serial.println("Measurement interval set to 15s");
  }

  error = scd30.setAltitudeCompensation(SCD30_ALTITUDE_COMPENSATION_M);
  if (error != NO_ERROR) {
    serial.print("Error setAltitudeCompensation(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
  } else {
    serial.print("Altitude compensation set to ");
    serial.println(SCD30_ALTITUDE_COMPENSATION_M);
  }

  error = scd30.startPeriodicMeasurement(0);
  if (error != NO_ERROR) {
    serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    serial.println(errorMessage);
    return;
  }
  serial.println("Periodic measurement started");

  // Wait for the first measurement to be ready (>1 interval at 15s).
  delay(20000);
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

  serial.println();
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
