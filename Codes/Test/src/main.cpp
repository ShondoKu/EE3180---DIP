#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include "ThingSpeak.h"
#define SoundSensorPin A0
#define VREF 3
#define PMS7003_TX 12
#define PMS7003_RX 14
#define PMS7003_PREAMBLE_1 0x42 // From PMS7003 datasheet
#define PMS7003_PREAMBLE_2 0x4D
#define PMS7003_DATA_LENGTH 31
#define BotToken "7706841902:AAG4sGQWfzoLxMo0NqCftb0wddZ9PjHv_ww" // https://t.me/SmartEnviroBot

SoftwareSerial _serial(PMS7003_TX, PMS7003_RX); // RX,TX
Adafruit_BME280 bme;

String tele_Message;
unsigned long bot_lasttime; // last time messages' scan has been done
const unsigned long BOT_MTBS = 1000;

unsigned long last_UploadTime;
const unsigned long uploadInterval = 600000; //cam change for showcase

unsigned long lastCheckHazardTime;
const unsigned long checkHazard = 10000; 

unsigned long lastHazardMessageTime;
const unsigned long hazardMessageInterval = 600000; //can change for showcase
bool hazardMessageSent = false;

int _pm1, _pm25, _pm10;
float _dB;
float _temperature, _humidity;

// const char* ssid = "Sean";
// const char* password = "whatpassword";

const char *ssid = "SHONDO 3544";
const char *password = "testing123";

// const char *ssid = "TP-LINK_0248";
// const char *password = "35353577";

String TS_apiKey = "EGLOW3A9ZEGHGOHD";
const char *TS_server = "api.thingspeak.com";

unsigned long myWriteChannelNumber = 2649409;
unsigned long myReadChannelNumber = 2660664;
const char *myWriteAPIKey = "EGLOW3A9ZEGHGOHD";
const char *myReadAPIKey = "OYBEVIRYYKNZXSAY"; // to ask theem for channel

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure client;
WiFiClient TS_client;
UniversalTelegramBot bot(BotToken, client);

// TelegramKeyboard keyboard_one; //todo if got time
int statusCode = 0;
int field[3] = {1, 2, 3};
float predicted[3] = {NAN, NAN, NAN}; // pm2.5,pm10,dB

void uploadCloud2(float dB, int pm1, int pm25, int pm10, float temp, float humid)
{

  ThingSpeak.setField(1, dB);
  ThingSpeak.setField(2, pm1);
  ThingSpeak.setField(3, pm25);
  ThingSpeak.setField(4, pm10);
  ThingSpeak.setField(5, temp);
  ThingSpeak.setField(6, humid);

  int x = ThingSpeak.writeFields(myWriteChannelNumber, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Channel update successful.");
  }
  else
  {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}

void readTempHumdSensor()
{
  _temperature = bme.readTemperature();
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
  voltageValue = analogRead(SoundSensorPin) / 1024.0 * VREF;
  _dB = voltageValue * 50.0;
  Serial.print("dB Value: ");
  Serial.print(_dB);
  Serial.println("  dB");
  Serial.println();
}
void readPMSSensor()
{
  int checksum = 0;
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
  readdBSensor();
  readPMSSensor();
  readTempHumdSensor();
  tele_Message = "🌡️ *Current Reading :*\n\n";                      // want to try to bold
  tele_Message += "Temperature: " + String(_temperature) + " °C\n"; //+ tempTrend + "\n";
  tele_Message += "Humidity: " + String(_humidity) + " %\n";        //+ humidityTrend + "\n";
  tele_Message += "PM2.5: " + String(_pm25) + " µg/m³\n";           //+ pm25Trend + "\n";
  tele_Message += "PM10: " + String(_pm10) + " µg/m³\n";            //+ pm10Trend + "\n";
  tele_Message += "Noise Level: " + String(_dB) + " dB\n";          // + noiseTrend + "\n";
}
void sendPredictedValuesToUser()
{

  for (int i = 0; i < sizeof(predicted); i++)
  {
    while (isnan(predicted[i]))
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
  tele_Message += "Predicted Noise: " + String(predicted[2]) + " dB\n";
  tele_Message += "\n[Predicted Gauges](https://thingspeak.mathworks.com/apps/matlab_visualizations/587793?height=auto&width=auto)\n";
}

void hazardCurrentValues()
{
  tele_Message = "⚠️ Hazardous Levels in the surronding area ⚠️\n";
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
  bot.sendMessage("824917767", tele_Message, "");
  hazardMessageSent = true;
  lastHazardMessageTime = millis();
}
void hazardPredictedValues()
{

  tele_Message = "⚠️ Potential Hazardous Levels in the surronding area ⚠️\n";
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

  bot.sendMessage("824917767", tele_Message, "");

  hazardMessageSent = true;
  lastHazardMessageTime = millis();
}

void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;

    if (text == "/start")
    {
      sendCurrentValuesToUser();
      bot.sendMessage(chat_id, tele_Message, "");
    }
    if (text == "/predicted")
    {
      sendPredictedValuesToUser();
      bot.sendMessage(chat_id, tele_Message, "");
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

  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  if (millis() - last_UploadTime >= uploadInterval)
  {
    readdBSensor();
    readPMSSensor();
    readTempHumdSensor();
    uploadCloud2(_dB, _pm1, _pm25, _pm10, _temperature, _humidity);
    last_UploadTime = millis();
  }
  if (millis() - lastCheckHazardTime >= checkHazard)
  {
    readdBSensor();
    readPMSSensor();
    sendPredictedValuesToUser();
    _dB = 85;
    if ((_dB >= 85.0 || _pm25 >= 55.5 || _pm10 >= 255.0) && !hazardMessageSent)
    {
      hazardCurrentValues();
    }
    else
    {
      if(millis() - lastHazardMessageTime >= hazardMessageInterval)
      {
        hazardMessageSent = false;
      }
    }
    if (predicted[2] >= 85.0 || predicted[0] >= 55.5 || predicted[1] >= 255.0)
    {
      hazardPredictedValues();
    }
    else
    {
      if(millis() - lastHazardMessageTime >= hazardMessageInterval)
      {
        hazardMessageSent = false;
      }
    }
    lastCheckHazardTime = millis();
  } 
  else if(hazardMessageSent)
  {

  }
}