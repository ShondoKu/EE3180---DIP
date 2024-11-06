#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "XXXXX";          // Replace with your Wi-Fi SSID
const char* password = "XXXXX";    // Replace with your Wi-Fi password

// ThingSpeak Read API credentials
const char* readAPIKey1 = "IHZH6WD3FMAM2NIA";  // Replace with the PM2.5 channel Read API Key
const char* channelID1 = "2649409";            // Channel ID for real-time data
const char* readAPIKey2 = "OYBEVIRYYKNZXSAY";  // Replace with the predicted data channel Read API Key
const char* channelID2 = "2660664";            // Channel ID for predicted data

// Telegram Bot Token and Chat ID
const char* botToken = "XXXXX";  // Replace with your Telegram bot token
const char* chatID = "XXXXX";               // Replace with your Telegram chat ID

WiFiClientSecure client;

// Previous values for trend analysis
float prevTemp = 0, prevHumidity = 0, prevPM25 = 0, prevPM10 = 0, prevNoise = 0;
// Previous predicted values for trend analysis
float prevPredictedPM25 = NAN;
float prevPredictedPM10 = NAN;
float prevPredictedNoise = NAN;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Attempt to connect to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Bypass SSL certificate verification (use with caution)
  client.setInsecure();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Separate HTTPClient instances for current and predicted data
    HTTPClient httpCurrent;
    HTTPClient httpPredicted;

    // Increase buffer size for larger JSON payloads
    DynamicJsonDocument doc(8192); // Increased size for more data
    DeserializationError error;

    // Fetch current data from ThingSpeak (Channel 1)
    String urlCurrent = "https://api.thingspeak.com/channels/" + String(channelID1) +
                        "/feeds.json?api_key=" + String(readAPIKey1) + "&results=1";
    Serial.println("Fetching current data from ThingSpeak...");
    httpCurrent.begin(client, urlCurrent);
    int httpResponseCodeCurrent = httpCurrent.GET();

    if (httpResponseCodeCurrent > 0) {
      String payloadCurrent = httpCurrent.getString();
      Serial.println("Current Data Payload:");
      Serial.println(payloadCurrent);

      error = deserializeJson(doc, payloadCurrent);

      if (error) {
        Serial.print("Error deserializing JSON: ");
        Serial.println(error.c_str());
      } else {
        // Extract current data
        float noise = doc["feeds"][0]["field1"].as<float>();
        float pm25 = doc["feeds"][0]["field3"].as<float>();
        float pm10 = doc["feeds"][0]["field4"].as<float>();
        float temperature = doc["feeds"][0]["field5"].as<float>();
        String humidityStr = doc["feeds"][0]["field6"].as<String>();
        float humidity = extractHumidityValue(humidityStr);

        // Determine trends for arrows
        String tempTrend = (temperature > prevTemp) ? "🔺" : (temperature < prevTemp) ? "🔻" : "➡️";
        String humidityTrend = (humidity > prevHumidity) ? "🔺" : (humidity < prevHumidity) ? "🔻" : "➡️";
        String pm25Trend = (pm25 > prevPM25) ? "🔺" : (pm25 < prevPM25) ? "🔻" : "➡️";
        String pm10Trend = (pm10 > prevPM10) ? "🔺" : (pm10 < prevPM10) ? "🔻" : "➡️";
        String noiseTrend = (noise > prevNoise) ? "🔺" : (noise < prevNoise) ? "🔻" : "➡️";

        // Update previous values
        prevTemp = temperature;
        prevHumidity = humidity;
        prevPM25 = pm25;
        prevPM10 = pm10;
        prevNoise = noise;

        // Create and send the current data message to Telegram
        String message = "🌡️ *Sensor Trends:*\n\n";
        message += "Temperature: " + String(temperature) + " °C " + tempTrend + "\n";
        message += "Humidity: " + String(humidity) + " % " + humidityTrend + "\n";
        message += "PM2.5: " + String(pm25) + " µg/m³ " + pm25Trend + "\n";
        message += "PM10: " + String(pm10) + " µg/m³ " + pm10Trend + "\n";
        message += "Noise Level: " + String(noise) + " dB " + noiseTrend + "\n";
        message += "\n[Current Gauges](https://thingspeak.mathworks.com/apps/matlab_visualizations/586875?height=auto&width=auto)";

        Serial.println("Sending current data message to Telegram...");
        sendMessageToTelegram(message);
      }
    } else {
      Serial.print("Error on HTTP request for current data: ");
      Serial.println(httpResponseCodeCurrent);
    }
    httpCurrent.end();  // End the httpCurrent session

    // Clear the JSON document before reusing it
    doc.clear();

    // Fetch predicted data from ThingSpeak (Channel 2)
    String urlPredicted = "https://api.thingspeak.com/channels/" + String(channelID2) +
                          "/feeds.json?api_key=" + String(readAPIKey2) + "&results=5";
    Serial.println("Fetching predicted data from ThingSpeak...");
    httpPredicted.begin(client, urlPredicted);
    int httpResponseCodePredicted = httpPredicted.GET();

    if (httpResponseCodePredicted > 0) {
      String payloadPredicted = httpPredicted.getString();
      Serial.println("Predicted Data Payload:");
      Serial.println(payloadPredicted);

      error = deserializeJson(doc, payloadPredicted);

      if (error) {
        Serial.print("Error deserializing JSON for predicted data: ");
        Serial.println(error.c_str());
      } else {
        // Extract predicted data
        float predictedPM25 = NAN;
        float predictedPM10 = NAN;
        float predictedNoise = NAN;

        int feedCount = doc["feeds"].size();

        // Loop through feeds starting from the latest
        for (int i = feedCount - 1; i >= 0; i--) {
          JsonObject feed = doc["feeds"][i];

          if (isnan(predictedPM25)) {
            const char* field1 = feed["field1"];
            if (field1 != NULL && strlen(field1) > 0) {
              predictedPM25 = atof(field1);
            }
          }

          if (isnan(predictedPM10)) {
            const char* field2 = feed["field2"];
            if (field2 != NULL && strlen(field2) > 0) {
              predictedPM10 = atof(field2);
            }
          }

          if (isnan(predictedNoise)) {
            const char* field3 = feed["field3"];
            if (field3 != NULL && strlen(field3) > 0) {
              predictedNoise = atof(field3);
            }
          }

          // Break if all values are found
          if (!isnan(predictedPM25) && !isnan(predictedPM10) && !isnan(predictedNoise)) {
            break;
          }
        }

        // Handle cases where predicted values are still NaN
        if (isnan(predictedPM25)) {
          predictedPM25 = prevPredictedPM25; // or set to a default value
        }

        if (isnan(predictedPM10)) {
          predictedPM10 = prevPredictedPM10; // or set to a default value
        }

        if (isnan(predictedNoise)) {
          predictedNoise = prevPredictedNoise; // or set to a default value
        }

        // Determine trends for predicted values
        String predictedPM25Trend = (predictedPM25 > prevPredictedPM25) ? "🔺" : (predictedPM25 < prevPredictedPM25) ? "🔻" : "➡️";
        String predictedPM10Trend = (predictedPM10 > prevPredictedPM10) ? "🔺" : (predictedPM10 < prevPredictedPM10) ? "🔻" : "➡️";
        String predictedNoiseTrend = (predictedNoise > prevPredictedNoise) ? "🔺" : (predictedNoise < prevPredictedNoise) ? "🔻" : "➡️";

        // Update previous predicted values
        prevPredictedPM25 = predictedPM25;
        prevPredictedPM10 = predictedPM10;
        prevPredictedNoise = predictedNoise;

        // Create and send the predicted data message to Telegram
        String predictedMessage = "🔮 *Average Predicted Sensor Trends:*\n\n";
        predictedMessage += "Predicted PM2.5: " + (isnan(predictedPM25) ? "N/A" : String(predictedPM25)) + " µg/m³ " + predictedPM25Trend + "\n";
        predictedMessage += "Predicted PM10: " + (isnan(predictedPM10) ? "N/A" : String(predictedPM10)) + " µg/m³ " + predictedPM10Trend + "\n";
        predictedMessage += "Predicted Noise Level: " + (isnan(predictedNoise) ? "N/A" : String(predictedNoise)) + " dB " + predictedNoiseTrend + "\n";
        predictedMessage += "\n[Predicted Gauges](https://thingspeak.mathworks.com/apps/matlab_visualizations/587793?height=auto&width=auto)";

        Serial.println("Sending predicted data message to Telegram...");
        sendMessageToTelegram(predictedMessage);
      }
    } else {
      Serial.print("Error on HTTP request for predicted data: ");
      Serial.println(httpResponseCodePredicted);
    }
    httpPredicted.end();  // End the httpPredicted session

    Serial.println("Delaying for 1 minute before the next request...");
    delay(60000);  // 1-minute delay
  } else {
    Serial.println("WiFi not connected, retrying...");
    delay(5000);  // 5-second delay before retrying Wi-Fi
  }
}

void sendMessageToTelegram(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // URL-encode the message
    String encodedMessage = urlencode(message);

    // Construct the URL with query parameters
    String telegramUrl = "https://api.telegram.org/bot" + String(botToken) +
                         "/sendMessage?chat_id=" + String(chatID) +
                         "&text=" + encodedMessage +
                         "&parse_mode=Markdown";

    Serial.println("Sending message to Telegram...");
    http.begin(client, telegramUrl);  // Initialize HTTPClient with the URL

    int httpResponseCode = http.GET();  // Send the GET request

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Message sent successfully");
      Serial.println("Telegram Response: " + response);
    } else {
      Serial.print("Error sending message: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

String urlencode(String str) {
  String encodedString = "";
  char c;
  char bufHex[4];
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        ('0' <= c && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encodedString += c;
    } else {
      sprintf(bufHex, "%%%02X", c);
      encodedString += bufHex;
    }
  }
  return encodedString;
}

float extractHumidityValue(String humidityStr) {
  // Extract numeric value from the string
  float humidityValue = 0.0;
  int len = humidityStr.length();
  String numericStr = "";

  for (int i = 0; i < len; i++) {
    char c = humidityStr.charAt(i);
    if (isDigit(c) || c == '.') {
      numericStr += c;
    }
  }

  if (numericStr.length() > 0) {
    humidityValue = numericStr.toFloat();
  }

  return humidityValue;
}
