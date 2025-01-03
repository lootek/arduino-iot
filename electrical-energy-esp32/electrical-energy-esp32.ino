#include <EspMQTTClient.h>
#include "/mnt/data/projects/arduino-playground/wifi-creds.h"
#include "EmonLib.h"
#include <driver/adc.h>

const bool debug = false;

#define ADC_INPUT 34
#define HOME_VOLTAGE 230.0

// #define ADC_BITS    12
// #define ADC_COUNTS  (1<<ADC_BITS)

EnergyMonitor emon1;

EspMQTTClient client(
  ssid_h,
  password,
  "192.168.10.18",      // MQTT Broker server ip
  "",                   // MQTTUsername, Can be omitted if not needed
  "",                   // MQTTPassword, Can be omitted if not needed
  "energy",             // Client name that uniquely identify your device
  1883                  // The MQTT port, default to 1883. this line can be omitted
);

void setup_mqtt()
{
  client.enableDebuggingMessages(debug);
  client.enableMQTTPersistence();
  client.setKeepAlive(90);
  client.enableLastWillMessage("/sensors/energy/lastwill", "I am going offline");
}

void onConnectionEstablished()
{
  client.publish("/sensors/energy/mqtt/status", "ready");
  client.publish("/sensors/energy/wifi/ip", String(client.getMqttServerIp()));
}

void setup_sct()
{
  Serial.begin(115200);

  analogReadResolution(12);

  emon1.current(ADC_INPUT, 50 * 2.2);
}

void measure(Stream& serial, int samples) {
  Serial.println("");

//  unsigned long time_diff, now = millis();
  double amps = emon1.calcIrms(samples);
//  time_diff = millis() - now;

//  Serial.print(time_diff);
//  Serial.print("ms for ");
//  Serial.print(samples);
//  Serial.println(" samples");

  Serial.print(amps);
  Serial.println("A");
  client.publish("/sensors/energy/sct013/current", String(amps));

  double watts = amps * HOME_VOLTAGE;

  Serial.print(watts);
  Serial.println("W");
  client.publish("/sensors/energy/sct013/power", String(watts));
}

void setup()
{
  Serial.begin(115200);
  setup_mqtt();
  setup_sct();
}

void loop()
{
  // measure(Serial, 1000);
  // measure(Serial, 1480);
  // measure(Serial, 2000);
  // measure(Serial, 5000);
  measure(Serial, 10000);
  // measure(Serial, 20000);
  // measure(Serial, 50000);

  // MQTT
  client.loop();

  if (debug) {
    delay(1 * 1000);
  } else {
    delay(30 * 1000);
  }
}
