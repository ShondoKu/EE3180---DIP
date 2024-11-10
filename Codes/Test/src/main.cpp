#include <Arduino.h>
#include <WiFiClientSecure.h> // used for telegram and ESP
#include <WiFiClient.h>       // used for TS
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SoftwareSerial.h> // used for PMS7003
#include <ArduinoJson.h>    // needed for Telebot library to work
#include <UniversalTelegramBot.h>
#include "ThingSpeak.h"

#define SoundSensorPin A0 // SEN0232 Pin
#define VREF 3            // voltage supplied to SEN0232 (db Sensor)

#define PMS7003_TX 12 // D6
#define PMS7003_RX 14 // D5

#define PMS7003_PREAMBLE_1 0x42 // From PMS7003 datasheet
#define PMS7003_PREAMBLE_2 0x4D
#define PMS7003_DATA_LENGTH 31

#define BotToken "7706841902:AAG4sGQWfzoLxMo0NqCftb0wddZ9PjHv_ww" // https://t.me/SmartEnviroBot // HIDE PLEASE!

SoftwareSerial _serial(PMS7003_TX, PMS7003_RX); // RX,TX

Adafruit_BME280 bme; // BME280 library (temp,humid sensor)

String tele_Message; // messages to send to user

unsigned long bot_lasttime;          // last time messages' scan has been done
const unsigned long BOT_MTBS = 1000; // no idea i copy from somewher

unsigned long last_UploadTime;
const unsigned long uploadInterval = 600000; // can change for showcase

unsigned long lastCheckHazardTime;
const unsigned long checkHazard = 10000; // check environment every 10s for any hazardous levels

unsigned long lastHazardMessageTime;
const unsigned long hazardMessageInterval = 600000; // can change for showcase // 10mins interval for hazard alerts
bool hazardMessageSent = false;                     // used to not spam the user for hazard alerts

int _pm1, _pm25, _pm10; // variables to store data after reading from sensors
float _dB;
float _temperature, _humidity;

// const char* ssid = "Sean";
// const char* password = "whatpassword"; //my phone hotspot

const char *ssid = "SHONDO 3544";
const char *password = "testing123"; // laptop hotspot

// const char *ssid = "TP-LINK_0248";
// const char *password = "35353577"; //hall wifi p

unsigned long myWriteChannelNumber = 2649409;   // channel where esp sends data to
unsigned long myReadChannelNumber = 2660664;    // channel to read predicted values
const char *myWriteAPIKey = "EGLOW3A9ZEGHGOHD"; // api for writing data
const char *myReadAPIKey = "OYBEVIRYYKNZXSAY";  // api for reading data

X509List cert(TELEGRAM_CERTIFICATE_ROOT);   // needed for telebot
WiFiClientSecure client;                    // for telebot
WiFiClient TS_client;                       // for thingspeak, cannot use same cause TS wont work
UniversalTelegramBot bot(BotToken, client); // telebot library

int statusCode = 0; // status code to check if data is can read/write. will be used to display error code if there is error that happens

float predicted[3] = {NAN, NAN, NAN}; // pm2.5,pm10,dB // to read data from predicted

void uploadCloud2(float dB, int pm1, int pm25, int pm10, float temp, float humid)
{

  ThingSpeak.setField(1, dB);
  ThingSpeak.setField(2, pm1);
  ThingSpeak.setField(3, pm25);
  ThingSpeak.setField(4, pm10);
  ThingSpeak.setField(5, temp);
  ThingSpeak.setField(6, humid); // set which data is for which field

  int x = ThingSpeak.writeFields(myWriteChannelNumber, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Channel update successful.");
  }
  else
  {
    Serial.println("Problem updating channel. HTTP error code " + String(x)); // send error code if have
  }
}

void readTempHumdSensor()
{
  _temperature = bme.readTemperature(); // from examples
  _humidity = bme.readHumidity();
  Serial.print("Temperature: ");
  Serial.print(_temperature);
  Serial.println("  °C");
  Serial.println();
  Serial.print("Humidity: ");
  Serial.print(_humidity);
  Serial.println("  %");
  Serial.println();
}
void readdBSensor()
{
  float voltageValue;
  voltageValue = analogRead(SoundSensorPin) / 1024.0 * VREF; // from datasheet of sen0232 //VREF is declared on top
  _dB = voltageValue * 50.0;
  Serial.print("dB Value: ");
  Serial.print(_dB);
  Serial.println("  dB");
  Serial.println();
}
void readPMSSensor()
{
  int checksum = 0; // THIS IS COPIED I DONT UNDERSTAND BUT IT WORKS
  unsigned char pms[32] = {
      0,
  };

  /**
   * Search preamble for Packet
   * Solve trouble caused by delay function
   */
  while (_serial.available() &&
         _serial.read() != PMS7003_PREAMBLE_1 &&
         _serial.peek() != PMS7003_PREAMBLE_2)
  {
  }
  if (_serial.available() >= PMS7003_DATA_LENGTH)
  {
    pms[0] = PMS7003_PREAMBLE_1;
    checksum += pms[0];
    for (int j = 1; j < 32; j++)
    {
      pms[j] = _serial.read();
      if (j < 30)
        checksum += pms[j];
    }
    _serial.flush();
    if (pms[30] != (unsigned char)(checksum >> 8) || pms[31] != (unsigned char)(checksum))
    {
      Serial.println("Checksum error");
      return;
    }
    if (pms[0] != 0x42 || pms[1] != 0x4d)
    {
      Serial.println("Packet error");
      return;
    }
    _pm1 = makeWord(pms[10], pms[11]);
    _pm25 = makeWord(pms[12], pms[13]);
    _pm10 = makeWord(pms[14], pms[15]);
  }
  Serial.print("PM1 Value: ");
  Serial.print(_pm1);
  Serial.println("  µg/m³");
  Serial.println();
  Serial.print("PM2.5 Value: ");
  Serial.print(_pm25);
  Serial.println("  µg/m³");
  Serial.println();
  Serial.print("PM10 Value: ");
  Serial.print(_pm10);
  Serial.println("  µg/m³");
  Serial.println();
}

void sendCurrentValuesToUser()
{
  readdBSensor(); // read from sensors
  readPMSSensor();
  readTempHumdSensor();

  tele_Message = "🌡️ *Current Reading :*\n\n";
  tele_Message += "Temperature: " + String(_temperature) + " °C\n";
  tele_Message += "Humidity: " + String(_humidity) + " %\n";
  tele_Message += "PM2.5: " + String(_pm25) + " µg/m³\n";
  tele_Message += "PM10: " + String(_pm10) + " µg/m³\n";
  tele_Message += "Noise Level: " + String(_dB) + " dB\n"; // set tele message
}
void sendPredictedValuesToUser()
{

  for (int i = 0; i < sizeof(predicted); i++) // do for all 3 readings
  {
    while (isnan(predicted[i])) // do while got any NAN still exist
    {
      predicted[i] = ThingSpeak.readFloatField(myReadChannelNumber, i + 1);
      statusCode = ThingSpeak.getLastReadStatus();
      if (statusCode != 200)
      {
        Serial.println("Problem reading channel. HTTP error code " + String(statusCode));
      }
    }
  }

  tele_Message = "🔮 *Predicted Environment!:*\n\n"; // want to try to bold
  tele_Message += "Predicted PM2.5: " + String(predicted[0]) + " µg/m³\n";
  tele_Message += "Predicted PM10: " + String(predicted[1]) + " µg/m³\n";
  tele_Message += "Predicted Noise: " + String(predicted[2]) + " dB\n"; // set tele message
}

void hazardCurrentValues()
{
  tele_Message = "⚠️ Hazardous Levels in the surronding area ⚠️\n"; //replaces the telemessage with hazard message
  if (_dB >= 85 && _dB <= 89.9)
  {
    tele_Message += "Current Readings at " + String(_dB) + " dB!\n" + "Do not be in the area for more than 8 hours!\n" + "Risk of hearing damage!\n";
  }
  if (_dB >= 90 && _dB <= 94.9)
  {
    tele_Message += "Current Readings at " + String(_dB) + " dB!\n" + "Do not be in the area for more than 2 hours!\n" + "Risk of hearing damage!\n";
  }
  if (_dB >= 95 && _dB <= 119.9)
  {
    tele_Message += "Current Readings at " + String(_dB) + " dB!\n" + "Do not be in the area for more than 15 minutes!\n" + "Risk of hearing damage!\n";
  }
  if (_dB >= 120)
  {
    tele_Message += "Current Readings at " + String(_dB) + " dB!\n" + "Area is not safe!\n" + "Immediate Pain and Hearing Damage at highest risk!\n";
  }

  if (_pm25 >= 55.5 && _pm25 <= 150.4)
  {
    tele_Message += "Current Readings at " + String(_pm25) + " µg/m³!\n" + "Currently in Unhealthy range (55.5 - 150.4)!\n" + "Wear a mask if you are sensitive!\n";
  }
  if (_pm25 >= 150.5 && _pm25 <= 250.4)
  {
    tele_Message += "Current Readings at " + String(_pm25) + " µg/m³!\n" + "Currently in Very Unhealthy range (150.5 - 250.4)!\n" + "Highly encouraged to wear a N95 mask!\n";
  }
  if (_pm25 >= 250)
  {
    tele_Message += "Current Readings at " + String(_pm25) + " µg/m³!\n" + "Area is not safe!\nCurrently in Hazardous Range (>250)!" + "Wear a N95 mask or stay indoors!\n";
  }

  if (_pm10 >= 255 && _pm10 <= 354)
  {
    tele_Message += "Current Readings at " + String(_pm10) + " µg/m³!\n" + "Currently in Unhealthy range (255 - 354)!\n" + "Wear a mask if you are sensitive!\n";
  }
  if (_pm10 >= 255 && _pm10 <= 424)
  {
    tele_Message += "Current Readings at " + String(_pm10) + " µg/m³!\n" + "Currently in Very Unhealthy range (255 - 424)!\n" + "Highly encouraged to wear a N95 mask!\n";
  }
  if (_pm10 >= 425)
  {
    tele_Message += "Current Readings at " + String(_pm10) + " µg/m³!\n" + "Area is not safe!\nCurrently in Hazardous Range (>425)!" + "Wear a N95 mask or stay indoors!\n";
  }

  //needed to hardcode this. was damn tiring

  bot.sendMessage("824917767", tele_Message, ""); // exclusive to my chat ONLY. // dont abuse my chatid pls
  hazardMessageSent = true; // if hazard message is sent alrd, set to true so that need to wait 10mins
  lastHazardMessageTime = millis(); // put the timing for the last hazard message
}
void hazardPredictedValues()
{

  tele_Message = "⚠️ Potential Hazardous Levels in the surronding area ⚠️\n"; //replaces the telemessage with hazard message
  if (predicted[2] >= 85 && predicted[2] <= 89.9)
  {
    tele_Message += "Predicted Readings at " + String(predicted[2]) + " dB!\n" + "Do not be in the area for more than 8 hours!\n" + "Risk of hearing damage!\n";
  }
  if (predicted[2] >= 90 && predicted[2] <= 94.9)
  {
    tele_Message += "Predicted Readings at " + String(predicted[2]) + " dB!\n" + "Do not be in the area for more than 2 hours!\n" + "Risk of hearing damage!\n";
  }
  if (predicted[2] >= 95 && predicted[2] <= 119.9)
  {
    tele_Message += "Predicted Readings at " + String(predicted[2]) + " dB!\n" + "Do not be in the area for more than 15 minutes!\n" + "Risk of hearing damage!\n";
  }
  if (predicted[2] >= 120)
  {
    tele_Message += "Predicted Readings at " + String(predicted[2]) + " dB!\n" + "Area is not safe!\n" + "Immediate Pain and Hearing Damage at highest risk!\n";
  }

  if (predicted[0] >= 55.5 && predicted[0] <= 150.4)
  {
    tele_Message += "Predicted Readings at " + String(predicted[0]) + " µg/m³!\n" + "Predicted readings at Unhealthy range (55.5 - 150.4)!\n" + "Wear a mask if you are sensitive!\n";
  }
  if (predicted[0] >= 150.5 && predicted[0] <= 250.4)
  {
    tele_Message += "Predicted Readings at " + String(predicted[0]) + " µg/m³!\n" + "Predicted readings at Very Unhealthy range (150.5 - 250.4)!\n" + "Highly encouraged to wear a N95 mask!\n";
  }
  if (predicted[0] >= 250)
  {
    tele_Message += "Predicted Readings at " + String(predicted[0]) + " µg/m³!\n" + "Area is not safe!\nPredicted readings at Hazardous Range (>250)!" + "Wear a N95 mask or stay indoors!\n";
  }

  if (predicted[1] >= 255 && predicted[1] <= 354)
  {
    tele_Message += "Predicted Readings at " + String(predicted[1]) + " µg/m³!\n" + "Predicted readings at Unhealthy range (255 - 354)!\n" + "Wear a mask if you are sensitive!\n";
  }
  if (predicted[1] >= 255 && predicted[1] <= 424)
  {
    tele_Message += "Predicted Readings at " + String(predicted[1]) + " µg/m³!\n" + "Predicted readings at Very Unhealthy range (255 - 424)!\n" + "Highly encouraged to wear a N95 mask!\n";
  }
  if (predicted[1] >= 425)
  {
    tele_Message += "Predicted Readings at " + String(predicted[1]) + " µg/m³!\n" + "Area is not safe!\nPredicted readings at Hazardous Range (>425)!" + "Wear a N95 mask or stay indoors!\n";
  }

  //shag is shag


  bot.sendMessage("824917767", tele_Message, ""); //please.

  hazardMessageSent = true; //same as current hazard 
  lastHazardMessageTime = millis();
}

void handleNewMessages(int numNewMessages)
{
  //referred from the telebot library
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id; //get the user chatid
    String text = bot.messages[i].text; //reads the user command

    if (text == "/start") //show all other commands with keyboard
    {
      String keyboardJson = "[[\"/Current\", \"/Predicted\"]]"; //keyboard design
      tele_Message = "Welcome to Smart Monitoring System (Noise & Dust)\nWhat do you want to see?\n";
      tele_Message += "/Current : Show the current environment Particulate Matter & dB\n";
      tele_Message += "/Predicted : Show the predicted environment Particulate Matter & dB\n";
      bot.sendMessageWithReplyKeyboard(chat_id, tele_Message, "", keyboardJson, true); //send messsage with keyboard. no idea why the true is there tho
    }
    if (text == "/Current") //send current values
    {
      sendCurrentValuesToUser(); //reads current values
      String keyboardJson = "[[{ \"text\" : \"Current Gauges\", \"url\" : \"https://thingspeak.mathworks.com/apps/matlab_visualizations/586875?height=auto&width=auto\" }]]"; //add the url below message
      bot.sendMessageWithInlineKeyboard(chat_id, tele_Message, "Markdown", keyboardJson); //send message with url
    }
    if (text == "/Predicted")
    {
      sendPredictedValuesToUser(); //reads predicted values //same as above just predicted
      String keyboardJson = "[[{ \"text\" : \"Predicted Gauges\", \"url\" : \"https://thingspeak.mathworks.com/apps/matlab_visualizations/587793?height=auto&width=auto\" }]]";
      bot.sendMessageWithInlineKeyboard(chat_id, tele_Message, "Markdown", keyboardJson); //same as above
    }
  }
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  _serial.begin(9600);
  bme.begin(0x76);
  configTime(0, 0, "pool.ntp.org");
  client.setTrustAnchors(&cert);
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // print a new line, then print WiFi connected and the IP address
  Serial.println("");
  Serial.println("WiFi connected");
  // Print the IP address
  // Serial.println(WiFi.localIP());
  // bot.begin();
  ThingSpeak.begin(TS_client);
}

void loop()
{

  if (millis() - bot_lasttime > BOT_MTBS) //every 1sec scan for message
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1); //reads last message

    while (numNewMessages) //copied from example
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1); //no clue the +1
    }

    bot_lasttime = millis();
  }
  if (millis() - last_UploadTime >= uploadInterval) //only do every 10mins, lmk if dont unds this
  {
    readdBSensor();
    readPMSSensor();
    readTempHumdSensor();
    uploadCloud2(_dB, _pm1, _pm25, _pm10, _temperature, _humidity);
    last_UploadTime = millis();
  }
  if (millis() - lastCheckHazardTime >= checkHazard) //do every 10sec
  {
    readdBSensor();
    readPMSSensor();
    sendPredictedValuesToUser();
    if ((_dB >= 85.0 || _pm25 >= 55.5 || _pm10 >= 255.0) && !hazardMessageSent) //this is for hazard values, last part of the condition is to avoid spamming //if hazardmsg is sent, then hazardmessage is True making this if not to work
    {
      hazardCurrentValues();
    }
    else
    {
      if (millis() - lastHazardMessageTime >= hazardMessageInterval) //after 10mins then hazardmessagesent is set to false so means can send hazard message again if still got hazardvalues around
      {
        hazardMessageSent = false;
      }
    }
    if (predicted[2] >= 85.0 || predicted[0] >= 55.5 || predicted[1] >= 255.0) //just for general checking every 10s
    {
      hazardPredictedValues();
    }
    else
    {
      if (millis() - lastHazardMessageTime >= hazardMessageInterval) //same as the above
      {
        hazardMessageSent = false;
      }
    }
    lastCheckHazardTime = millis(); //go online to see what is millis() if dont understand
  }
}