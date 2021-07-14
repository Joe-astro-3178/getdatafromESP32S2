// Copied from Ian on 4/06/2021 by Joe
// Modified by Joe on 7/06/2021 to incorporate pin 7 code from Alan 
// Modified by Joe on 8/06/2021 to make the wind sensor run on pin 7.

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESPDateTime.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_NeoPixel.h>
#include "wificred.h"               //Your WiFi Credentials file (required to be set up by the user and placed in same directory)
//#include "wificredmensshed.h"       //Men's Shed WiFi Credentials file (required to be set up by the user and placed in same directory)

#define PIN 18                      // Default pin for the Built-in RGD LED
#define NUMPIXELS 1                 // Only 1 RGB LED

const int analogInPin = 7;          // Analog input pin that the potentiometer is attached to (for Wind Speed sensor)
int sensorValue = 0;                // value read from the pot
int outputValue = 0;
int highestsensorValue = 0;
int highestoutputValue = 0;
long lastMsg = 0;
char msg[50];
int value = 0;
int reporttime = 600000;            // Reporting time period used in the 'delaytime' command in milliseconds (can be changed via MQ)
int resetreporttime = 600000;       // This variable is used to validate the new reporting period sent via MQ 
int defaulttime = 600000;           // Default reporting is 600000 = 10 min

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);   // Initialize the NeoPixel
Adafruit_BME280 bme;                // Initialize BME Sensor

char buffer[256];
StaticJsonDocument<256> payload;
WiFiClient espClient;
PubSubClient client(espClient);

 
void setup_wifi(){
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      Serial.print("Establishing connection to WiFi...");
      Serial.println(ssid);
    }
    Serial.println("Connected to network");
    Serial.print("MAC: ");    
    Serial.print(WiFi.macAddress());
    Serial.print(", IP: ");
    Serial.println(WiFi.localIP());
}


void setupDateTime() {
  // setup this after wifi connected
  // you can use custom timeZone,server and timeout
  DateTime.setServer("1.au.pool.ntp.org");
  DateTime.setTimeZone("AEST-10AEDT,M10.1.0,M4.1.0/3");
  DateTime.begin();
  if (!DateTime.isTimeValid()) {
    Serial.println("Failed to get time from server.");
  }
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  resetreporttime = messageTemp.toInt();      
  // Must be in millseconds between 1000 (1 sec) to defaulttime otherwise too many MQ messages will be sent if less
  // Do some validation here. If message contained non-numeric chars then 0 is returned in the .toInt() function above
  if (resetreporttime < 1000 || resetreporttime > defaulttime) {
    Serial.print(" Entry not Not Valid resetting to default reporting time : ");
    resetreporttime = defaulttime;
  } 
  else {
    Serial.print(" New reporting time period reset to: ");
  }
  Serial.print(resetreporttime/1000);  
  Serial.println(" seconds");
}


void reconnect() {
  // Loop until we're reconnected
  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    Serial.print(mqttserver);
    // Attempt to connect
    if (client.connect("espClient")) {
      Serial.println(" connected");
      // Subscribe
      client.subscribe("Output");
    } else {
      pixels.setPixelColor(0, 255,255,255); // Set LED to White for 1 sec to adviser user of no MQ connect
      pixels.show();
      delay(1000);
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      pixels.setPixelColor(0, 255,2,2);     // Set LED to Red for 5 sec to adviser user of no MQ connect
      pixels.show();                        // If LED white for 1 sec and red for 5 sec continues, then reset make be required
      delay(5000);
    }
  }
}


void getData(){
      float temperature;
      float humidity;
      float pressure;

      temperature = bme.readTemperature();       // Get Temperature in Celsius
      humidity = bme.readHumidity();             // Get Humitity  
      pressure = (bme.readPressure())/100;       // Get Pressure

      payload["Temperature"] = temperature;
      payload["Humidity"] = humidity;
      payload["Pressure"] = pressure;
      payload["DateTime"] = DateTime.now();
      payload["POTSensor"] = highestsensorValue; // This value is established in the loop() routine
      payload["POTOutput"] = highestoutputValue; // This value is established in the loop() routine
  
      size_t n = serializeJson(payload, buffer);
      client.publish("weather", buffer ,n);
      
      Serial.print("Date & Time: ");
      Serial.print(DateTime.toString());
      Serial.print(", Temp: ");
      Serial.print(temperature);
      Serial.print(", Humidity: ");
      Serial.print(humidity);
      Serial.print(", Pressure: ");
      Serial.print(pressure);
      Serial.print(", Wind Sensor: ");
      Serial.print(highestsensorValue);
      Serial.print(", Wind Speed: ");
      Serial.print(highestoutputValue);
      Serial.print(", Linux Date: ");
      Serial.println(DateTime.now());
}


void setup() {
  Wire.begin(15, 16);                     //Start the GPIO connection using pins 15 and 16 for the BME Sensor
  pixels.setBrightness(10);
  pixels.begin();                         //Start the NeoPixe;
  Serial.begin(115200);
  
  setup_wifi();
  setupDateTime();
  
  client.setServer(mqttserver, 1883);
  client.setCallback(callback);           // This is automatically called when a message arrives

  if (!bme.begin(0x76, &Wire)) {          //Start the BME Sensor using the PINS sepecified when the Wire was started
   Serial.println("Could not find a valid BME280 sensor, check wiring!");
   while (1);
  }
  else {
    Serial.println("Connected to BME280");
  }
  if (!client.connected()) {
    reconnect();
  }
  Serial.print("Reporting sensor readings every: ");
  Serial.print(reporttime/1000);  
  Serial.println(" seconds");
  getData();
}

void loop() {
  reporttime = resetreporttime;            // Reset reporting time period from subscribed MQ message
  
  sensorValue = analogRead(analogInPin);   // read the analog in value
  outputValue = map(sensorValue, 0, 3700, 0, 101);   // convert it to equivalent kph (as per approx. Joe's conversion)
  if (sensorValue > highestsensorValue) {
    highestsensorValue = sensorValue;      // Save Highest Sensor and Output Value during testing period
    highestoutputValue = outputValue;      // This is done becasue the wind is blustery and we want a steady figure
  }
// Set the LED to various colours depending on the sensor value. This is useful for testing purposes..............
  if (highestsensorValue < 700)        {
    pixels.setPixelColor(0, 255,255,2);    // Yellow (default) < 20kph
  } else if (highestsensorValue < 1500) {
    pixels.setPixelColor(0, 2,255,2);      // Green   < 41 kph
  } else if (highestsensorValue < 2300) {
    pixels.setPixelColor(0, 2,2,255);      // Blue    < 63 kph
  } else if (highestsensorValue < 3100) {
     pixels.setPixelColor(0, 255,2,2);     // Red     < 85 kph
  } else                                {
     pixels.setPixelColor(0, 255,255,255); // White   > 85 kph
  }
// Comment out above LED lines once testing is finished...........................................................
// Uncomment the following line after testing is finished to turn off the LED...................................... 
//  pixels.setPixelColor(0, pixels.Color(0, 0, 0));  //Set the NeoPixel colour - turning the LED off
  pixels.show();                          // enable the LED with the set colour
  
  if (!client.connected()) {              // check that MQ is still connected
    reconnect();
  }
  client.loop();
  long now = millis();
    
  if (now - lastMsg > reporttime) {       // Check if it's time to report sensor readings via MQTT
      lastMsg = now;
      getData();   
      highestsensorValue = 0;             // Reset highest vlue for next reporting period
      highestoutputValue = 0;             // Reset highest vlue for next reporting period
  }
delay(500);     // Wait 1/2 sec before repeating sensor checks
}
