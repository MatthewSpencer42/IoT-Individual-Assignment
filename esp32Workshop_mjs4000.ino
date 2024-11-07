#include "WiFi.h"
#include <DHT.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// --------------------------------------------------------------------------------------------
//        UPDATE CONFIGURATION TO MATCH YOUR ENVIRONMENT
// --------------------------------------------------------------------------------------------

// Add GPIO pins used to connect devices
int red = 22;
int green = 32;
int blue = 33;
#define DHT_PIN 23 // GPIO pin the data line of the DHT sensor is connected to

// Specify DHT11 (Blue) or DHT22 (White) sensor
#define DHTTYPE DHT11
//#define NEOPIXEL_TYPE NEO_RGB + NEO_KHZ800

// Temperatures to set LED by (assume temp in C)
#define ALARM_COLD 0.0
#define ALARM_HOT 30.0
#define WARN_COLD 10.0
#define WARN_HOT 25.0


// Add WiFi connection information
char ssid[] = "Network";  // your network SSID (name)
char pass[] = "Password";  // your network password

// MQTT connection details
#define MQTT_HOST "broker.mqtt.cool"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "d:hwu:esp8266:< mjs4000 >"
#define MQTT_USER "" // no need for authentication, for now
#define MQTT_TOKEN "" // no need for authentication, for now
#define MQTT_TOPIC "< mjs4000 >/evt/status/fmt/json"
#define MQTT_TOPIC_DISPLAY "< mjs4000 >/cmd/display/fmt/json"
#define MQTT_TOPIC_INTERVAL "< mjs4000 >/cmd/interval/fmt/json"

// --------------------------------------------------------------------------------------------
//        SHOULD NOT NEED TO CHANGE ANYTHING BELOW THIS LINE
// --------------------------------------------------------------------------------------------
DHT dht(DHT_PIN, DHTTYPE);

// variables to hold data
StaticJsonDocument<100> jsonDoc;
JsonObject payload = jsonDoc.to<JsonObject>();
JsonObject status = payload.createNestedObject("d");
StaticJsonDocument<100> jsonReceiveDoc;
static char msg[50];

float h = 0.0; // humidity
float t = 0.0; // temperature
int redBrightness = 0;
int greenBrightness = 0;
int blueBrightness = 0;
int ReportingInterval = 10;  // Reporting Interval seconds

// MQTT objects
void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  
  payload[length] = 0; // ensure valid content is zero terminated so can treat as c-string
  Serial.println((char *)payload);
  DeserializationError err = deserializeJson(jsonReceiveDoc, (char *)payload);
  if (err) {
    Serial.print(F("deserializeJson() failed with code ")); 
    Serial.println(err.c_str());
  } else {
    JsonObject cmdData = jsonReceiveDoc.as<JsonObject>();
    if (0 == strcmp(topic, MQTT_TOPIC_DISPLAY)) {
      //valid message received
      redBrightness = cmdData["r"]; // this form allows you specify the type of the data you want from the JSON object
      greenBrightness = cmdData["g"];
      blueBrightness = cmdData["b"];
      jsonReceiveDoc.clear();
      blueBrightness = 255 - blueBrightness;
      redBrightness = 255 - redBrightness;
      greenBrightness = 255 - greenBrightness;
      analogWrite(red, redBrightness);
      analogWrite(green, greenBrightness);
      analogWrite(blue, blueBrightness);
      
    } else if (0 == strcmp(topic, MQTT_TOPIC_INTERVAL)) {
      //valid message received
      ReportingInterval = cmdData["Interval"]; // this form allows you specify the type of the data you want from the JSON object
      Serial.print("Reporting Interval has been changed:");
      Serial.println(ReportingInterval);
      jsonReceiveDoc.clear();
    } else {
      Serial.println("Unknown command received");
    }
  }
}

void setup() {
  // put your setup code here, to run once
  // Start serial console
  Serial.begin(115200);
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);
  Serial.setTimeout(2000);
  while (!Serial) { }
  Serial.println();
  Serial.println("ESP8266 Sensor Application");

  // Start WiFi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Connected");

  // Start connected devices
  dht.begin();
  //pixel.begin(); 

    // Connect to MQTT broker
  if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
    Serial.println("MQTT Connected");
    mqtt.subscribe(MQTT_TOPIC_DISPLAY);
    mqtt.subscribe(MQTT_TOPIC_INTERVAL);

  } else {
    Serial.println("MQTT Failed to connect!");
    ESP.restart();
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  mqtt.loop();
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial.println("MQTT Connected");
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
      mqtt.subscribe(MQTT_TOPIC_INTERVAL);
      mqtt.loop();
    } else {
      Serial.println("MQTT Failed to connect!");
      delay(5000);
    }
  }
  h = dht.readHumidity();
  t = dht.readTemperature(); // uncomment this line for Celsius
  // t = dht.readTemperature(true); // uncomment this line for Fahrenheit

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    // Set RGB LED Colour based on temp (0 for want off, 255 for want on)
    blueBrightness = (t < ALARM_COLD) ? 255 : ((t < WARN_COLD) ? 150 : 0);
    redBrightness = (t >= ALARM_HOT) ? 255 : ((t > WARN_HOT) ? 150 : 0);
    greenBrightness = (t > ALARM_COLD) ? ((t <= WARN_HOT) ? 255 : ((t < ALARM_HOT) ? 150 : 0)) : 0;
    blueBrightness = 255 - blueBrightness;
    redBrightness = 255 - redBrightness;
    greenBrightness = 255 - greenBrightness;
    analogWrite(red, redBrightness);
    analogWrite(green, greenBrightness);
    analogWrite(blue, blueBrightness);    

    // Print Message to console in JSON format
    status["temp"] = t;
    status["humidity"] = h;
    serializeJson(jsonDoc, msg, 50);
    Serial.println(msg);
    if (!mqtt.publish(MQTT_TOPIC, msg)) {
      Serial.println("MQTT Publish failed");
    }
  }
  Serial.print("ReportingInterval :");
  Serial.print(ReportingInterval);
  Serial.println();
    // Pause - but keep polling MQTT for incoming messages
  for (int i = 0; i < ReportingInterval; i++) {
    mqtt.loop();
    delay(1000);
  }

}
