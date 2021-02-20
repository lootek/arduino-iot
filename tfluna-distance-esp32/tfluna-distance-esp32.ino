// Official docs: https://m.media-amazon.com/images/I/A1WJkVhXsEL.pdf

#include <driver/uart.h>
#include "WiFi.h"
#include "EspMQTTClient.h"

const char* ssid = "<SSID>";
const char* password = "<PASS>";

const bool debug = false;

EspMQTTClient client(
  ssid,
  password,
  "192.168.10.18",      // MQTT Broker server ip
  "",                   // MQTTUsername, Can be omitted if not needed
  "",                   // MQTTPassword, Can be omitted if not needed
  "esp32_septic_tank",  // Client name that uniquely identify your device
  1883                  // The MQTT port, default to 1883. this line can be omitted
);

void setup_wifi()
{
  bool just_connected = false;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");

    WiFi.begin(ssid, password);
    just_connected = true;
  }

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  if (debug) {
    Serial.println("");
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
  }

  if (just_connected) client.publish("/sensors/septic/wifi/ip", WiFi.localIP().toString());
}

void setup_mqtt()
{
  if (debug) {
    client.enableDebuggingMessages();
    client.enableHTTPWebUpdater();
    client.enableLastWillMessage("/sensors/septic/lastwill", "I am going offline");
  }
}

void onConnectionEstablished()
{
  client.publish("/sensors/septic/mqtt/status", "ready");
}

void setup_tfmini()
{
  uart_set_pin(UART_NUM_0, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  delay(100);

  // Set frequency to 0Hz (ie. "trigger mode")
  Serial.write((uint8_t)0x5A);
  Serial.write((uint8_t)0x06);
  Serial.write((uint8_t)0x03);
  Serial.write((uint8_t)0x00); // freq lowest byte here (eg 0A for 10Hz)
  Serial.write((uint8_t)0x00);
  Serial.write((uint8_t)0x00);

  delay(100);
}

void setup()
{
  Serial.begin(115200);

  setup_tfmini();
  setup_wifi();
  setup_mqtt();
}

void trigger(Stream& serial) {
  Serial.write((uint8_t)0x5A);
  Serial.write((uint8_t)0x04);
  Serial.write((uint8_t)0x04);
  Serial.write((uint8_t)0x00);
}

void measure(Stream& serial) {
  int distance; //actual distance measurements of LiDAR
  int strength; //signal strength of LiDAR
  float temperature;
  int check; //save check value
  int i;
  int uart[9]; //save data measured by LiDAR
  const int HEADER = 0x59; //frame header of data package

  Serial.println("check if serial port has data input");
  while (!serial.available()) { //check if serial port has data input
    delay(10);
  }

  Serial.println("waiting for header");
  while (serial.read() != HEADER) { //assess data package frame header 0x59
    delay(10);
  }

  uart[0] = HEADER;
  Serial.println("reading data");
  if (serial.read() == HEADER) { //assess data package frame header 0x59
    uart[1] = HEADER;
    for (i = 2; i < 9; i++) { //save data in array
      uart[i] = serial.read();
    }
    check = uart[0] + uart[1] + uart[2] + uart[3] + uart[4] + uart[5] + uart[6] + uart[7];
    if (uart[8] == (check & 0xff)) { //verify the received data as per protocol
      distance = uart[2] + uart[3] * 256; //calculate distance value
      strength = uart[4] + uart[5] * 256; //calculate signal strength value
      temperature = uart[6] + uart[7] * 256; //calculate chip temperature
      temperature = temperature / 8 - 256;

      client.publish("/sensors/septic/tfluna/distance", String(distance));
      client.publish("/sensors/septic/tfluna/signal-strength", String(strength));
      client.publish("/sensors/septic/tfluna/chip-temperature", String(temperature));

      if (debug) {
        Serial.print("distance = ");
        Serial.print(distance); //output measure distance value of LiDAR
        Serial.print('\t');
        Serial.print("strength = ");
        Serial.print(strength); //output signal strength value
        Serial.print("\t Chip temperature = ");
        Serial.print(temperature);
        Serial.println(" celcius degree"); //output chip temperature of Lidar
      }
    }
  }
}

void loop()
{
  // reconnect WiFi if needed
  setup_wifi();

  // perform measurement
  trigger(Serial);
  delay(10);
  measure(Serial);

  // MQTT
  client.loop();

  if (debug) {
    delay(2 * 1000);
  } else {
    delay(20 * 1000);
  }
}
