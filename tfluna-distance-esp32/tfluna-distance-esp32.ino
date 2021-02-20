#include <driver/uart.h>
#include <SoftwareSerial.h>

#include "WiFi.h"
#include "EspMQTTClient.h"
#include "TFMini.h"

const char* ssid = "<SSID>";
const char* password = "<PASS>";

const bool debug = true;

TFMini tfmini;
#define TFMINI_DEBUGMODE 1
#define TFMINI_MAX_MEASUREMENT_ATTEMPTS 100
#define TFMINI_FRAME_SIZE 9
#define TFMINI_MAXBYTESBEFOREHEADER 100

SoftwareSerial mySerial(16, 17);

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

  if (just_connected) client.publish("sensors/septic/wifi/ip", WiFi.localIP().toString());
}

void setup_mqtt()
{
  if (debug) {
    client.enableDebuggingMessages();
    client.enableHTTPWebUpdater();
    client.enableLastWillMessage("sensors/septic/lastwill", "I am going offline");
  }
}

void onConnectionEstablished()
{
  client.publish("sensors/septic/mqtt/status", "ready");
}

void setup_tfmini()
{
  mySerial.begin(TFMINI_BAUDRATE);
  tfmini.begin(&mySerial);
  delay(100);
//  tfmini.setSingleScanMode();
}

void setup()
{
  Serial.begin(115200);

  setup_tfmini();
  setup_wifi();
  setup_mqtt();
}

void measure() {
  tfmini.externalTrigger();

//  uint16_t dist = tfmini.getDistance();
//  uint16_t strength = tfmini.getRecentSignalStrength();
//
//  if (debug) {
//    Serial.print("triggered - ");
//    Serial.print(dist);
//    Serial.print(",");
//    Serial.println(strength);
//  }
}

void low_level_continuous_measure() {
  int distance; //actual distance measurements of LiDAR
  int strength; //signal strength of LiDAR
  float temperature;
  int check; //save check value
  int i;
  int uart[9]; //save data measured by LiDAR
  const int HEADER = 0x59; //frame header of data package

  Serial.println("check if serial port has data input");
  if (mySerial.available()) { //check if serial port has data input
    if (mySerial.read() == HEADER) { //assess data package frame header 0x59
      uart[0] = HEADER;
      if (mySerial.read() == HEADER) { //assess data package frame header 0x59
        uart[1] = HEADER;
        for (i = 2; i < 9; i++) { //save data in array
          uart[i] = mySerial.read();
        }
        check = uart[0] + uart[1] + uart[2] + uart[3] + uart[4] + uart[5] + uart[6] + uart[7];
        if (uart[8] == (check & 0xff)) { //verify the received data as per protocol
          distance = uart[2] + uart[3] * 256; //calculate distance value
          strength = uart[4] + uart[5] * 256; //calculate signal strength value
          temperature = uart[6] + uart[7] * 256; //calculate chip temperature
          temperature = temperature / 8 - 256;

          client.publish("sensors/septic/tfluna/distance", String(distance));
          client.publish("sensors/septic/tfluna/strength", String(strength));
          client.publish("sensors/septic/tfluna/temperature", String(temperature));

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
  }
}

void loop()
{
  setup_wifi();
  
//  measure();
  low_level_continuous_measure();
  
  client.loop();
  
  delay(1000);
}
