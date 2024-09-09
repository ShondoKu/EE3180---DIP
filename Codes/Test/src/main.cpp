#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "MQ135.h"

const int MQ135pin = A0;


const char* ssid = "TP-LINK_0248";
const char* password = "35353577";
String TS_apiKey = "EGLOW3A9ZEGHGOHD";
const char* TS_server = "api.thingspeak.com";

WiFiClient client;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
  }
  //print a new line, then print WiFi connected and the IP address
  Serial.println("");
  Serial.println("WiFi connected");
  // Print the IP address
  //Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:

  MQ135 gasSensor = MQ135(A0);
  float air_quality = gasSensor.getPPM();
  
  //float ppm = 1.0 / (0.03588 * pow(resistance, 1.336));
  Serial.print("Air Quality: ");  
  Serial.print(air_quality);
  Serial.println("  PPM");   
  Serial.println();

  if (client.connect(TS_server, 80))
  {
    String postStr = TS_apiKey;
    postStr += "&field1=";
    postStr += String(air_quality);
    postStr += "r\n";
    
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + TS_apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    
    Serial.println("Data Send to Thingspeak");
  }
    client.stop();
    Serial.println("Waiting...");
 
    delay(2000); 
  
}