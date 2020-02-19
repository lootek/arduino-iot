#include "WiFi.h"
#include "EspMQTTClient.h"
#include <driver/uart.h>

const char* ssid = "<SSID>";
const char* password = "<PASS>";

const bool debug = false;

EspMQTTClient client(
  ssid,
  password,
  "192.168.0.90",       // MQTT Broker server ip
  "",                   // MQTTUsername, Can be omitted if not needed
  "",                   // MQTTPassword, Can be omitted if not needed
  "septic_tank_esp32",  // Client name that uniquely identify your device
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

  if (just_connected) client.publish("sensors/septic/wifi", WiFi.localIP().toString());
}

void setup_mqtt()
{
  if (debug) {
    client.enableDebuggingMessages();
    client.enableHTTPWebUpdater();
    client.enableLastWillMessage("lastwill", "I am going offline");
  }
}

void onConnectionEstablished()
{
  client.publish("sensors/septic/mqtt", "ready");
}

void setup_uart_sensor()
{
  uart_set_pin(UART_NUM_0, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  pinMode(21, OUTPUT); 
}

void setup()
{
  Serial.begin(9600);

  setup_uart_sensor();
  setup_wifi();
  setup_mqtt();
}

void measure()
{
  digitalWrite(21, HIGH);

  unsigned char data[4] = {};
  const int max_measurement_attempts = 25;
  int successful_measurements = 0;
  float avg_distance = 0.;
  float measurements[max_measurement_attempts];

  for (int n = 0; n < max_measurement_attempts; n++) {
    do {
      for (int i = 0; i < 4; i++) {
        data[i] = Serial.read();
      }
    } while (Serial.read() == 0xff);
  
  //  Serial.flush();
  
    if (data[0] == 0xff) {
      int sum = (data[0] + data[1] + data[2]) & 0x00FF;
      if (sum == data[3]) {
        float distance = (data[1] << 8) + data[2];
        if (distance > 30) {

          if (debug) {
            Serial.print("distance=");
            Serial.print(distance / 10);
            Serial.println("cm");
          }

          distance /= 10;
          client.publish("sensors/septic/level/value", String(distance));
          
          avg_distance += distance;
          measurements[successful_measurements] = distance;
          successful_measurements++;
          
        } else {
          if (debug) Serial.println("Below the lower limit");
        }
      } else {
        if (debug) {
          Serial.print("ERROR: ");
          Serial.print(sum);
          Serial.print(" ");
          Serial.print(data[3]);
          Serial.println("");
        }
      }
    }
  
    // TODO: Reconnect WiFi?

    delay(100);
  }

  if (successful_measurements > 0) {
    avg_distance /= successful_measurements;
  
    float normalized_avg_distance = 0.;
    int normalized_measurements_count = 0;
    
    if (avg_distance > 0) {
      for (int i = 0; i < successful_measurements; i++) {
        if ((measurements[i] < avg_distance && measurements[i]/avg_distance > 0.9) 
        || (measurements[i] > avg_distance && avg_distance/measurements[i] > 0.9)) {
          normalized_avg_distance += measurements[i];
          normalized_measurements_count++;
        }
      }

      if (debug) {
        Serial.print("normalized_measurements_count=");
        Serial.print(normalized_measurements_count);
      }

      if (normalized_measurements_count > 0) {
        normalized_avg_distance /= normalized_measurements_count;
  
        if (debug) {
          Serial.print("normalized_avg_distance=");
          Serial.print(normalized_avg_distance);
        }
  
        client.publish("sensors/septic/level/avg", String(normalized_avg_distance));
        client.publish("sensors/septic/level/measurements", String(successful_measurements));
      }
    }
  }
  
  digitalWrite(21, LOW);
}


void loop()
{
  setup_wifi();
  measure();
  client.loop();
  delay(10 * 1000);
}
