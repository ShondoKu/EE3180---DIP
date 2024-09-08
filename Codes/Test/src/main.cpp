#include <Arduino.h>
#include <ESP8266WiFi.h>


#define LED 2
const char* ssid = "TP-LINK_0248";
const char* password = "35353577";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  WiFi.begin(ssid,password);
  pinMode(LED, OUTPUT);
  while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
  }
  //print a new line, then print WiFi connected and the IP address
  Serial.println("");
  Serial.println("WiFi connected");
  // Print the IP address
  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(LED, HIGH);
  Serial.println("LED is on");
  delay(500);
  digitalWrite(LED, LOW);
  Serial.println("LED is off");
  delay(500);
}