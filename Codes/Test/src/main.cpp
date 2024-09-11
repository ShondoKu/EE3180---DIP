#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#define SoundSensorPin A0
#define VREF 3
#define PMS7003_TX D5
#define PMS7003_RX D6
#define PMS7003_PREAMBLE_1  0x42 // From PMS7003 datasheet
#define PMS7003_PREAMBLE_2  0x4D
#define PMS7003_DATA_LENGTH 31

SoftwareSerial _serial(PMS7003_TX, PMS7003_RX); //RX,TX
int _pm1,_pm25,pm10;


const char* ssid = "Sean";
const char* password = "whatpassword";
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
  float voltageValue, dB;
  voltageValue = analogRead(SoundSensorPin) / 1024.0 * VREF;
  dB = voltageValue * 50.0;
  Serial.print("dB Value: ");  
  Serial.print(dB);
  Serial.println("  dB");   
  Serial.println();

  int checksum = 0;
  unsigned char pms[32] = {0,};

  /**
   * Search preamble for Packet
   * Solve trouble caused by delay function
   */
  while( _serial.available() && 
      _serial.read() != PMS7003_PREAMBLE_1 &&
      _serial.peek() != PMS7003_PREAMBLE_2 ) {
  }		
  if( _serial.available() >= PMS7003_DATA_LENGTH ){
    pms[0] = PMS7003_PREAMBLE_1;
    checksum += pms[0];
    for(int j=1; j<32 ; j++){
      pms[j] = _serial.read();
      if(j < 30)
        checksum += pms[j];
    }
    _serial.flush();
    if( pms[30] != (unsigned char)(checksum>>8) 
      || pms[31]!= (unsigned char)(checksum) ){
      Serial.println("Checksum error");
      return;
    }
    if( pms[0]!=0x42 || pms[1]!=0x4d ) {
      Serial.println("Packet error");
      return;
    }
    _pm1  = makeWord(pms[10],pms[11]);
    _pm25 = makeWord(pms[12],pms[13]);
    _pm10 = makeWord(pms[14],pms[15]);
  }		
 

  if (client.connect(TS_server, 80))
  {
    String postStr = TS_apiKey;
    postStr += "&field1=";
    postStr += String(dB);
    postStr += "&field2=";
    postStr += String(_pm1);
    postStr += "&field3=";
    postStr += String(_pm25);
    postStr += "&field4=";
    postStr += String(_pm10);
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

