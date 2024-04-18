#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include "config.h" // Configuration file
#include "time.h"

const int dry = 2000; // value for dry sensor
const int wet = 1000; // value for wet sensor
const int tooWet = 500; // value for too wet sensor
boolean watering = false; // value for too wet sensor

constexpr int RELAY_PIN  = 2; // Relay pin for pump

unsigned long startedWatering;
unsigned long lastTimeLoki = 0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;

// NTP Client
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP);

// Loki & Infllux Clients
HTTPClient httpInflux;

void stopWatering ();
void startWatering ();
void submitToLoki(float hum1, float hum2);
unsigned long getTime();


void setupWiFi() {
  delay(1000);
  Serial.print("Connecting to '");
  Serial.print(WIFI_SSID);
  Serial.print("' ...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void setup()
{ 
  Serial.begin(9600);

  // Connect to WiFi
  setupWiFi();

  // Initialize a NTPClient to get time
  configTime(gmtOffset_sec, 0, ntpServer);
  lastTimeLoki = millis() - INTERVAL;

  pinMode(RELAY_PIN, OUTPUT);

  startWatering();
  delay(1000);
  stopWatering();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    yield();
    setupWiFi();
  }

  int sensorVal = analogRead(34);

  // Sensor has a range of 239 to 595
  // We want to translate this to a scale or 0% to 100%
  // More info: https://www.arduino.cc/reference/en/language/functions/math/map/
  int percentageHumididy = map(sensorVal, wet, dry, 100, 0); 


  //turn the pump on for x seconds
  if (percentageHumididy < 50 && millis() - startedWatering > 10 * 1000) {
    startWatering();
  } else if (percentageHumididy > 50) {
    stopWatering();
  }
  

  if (millis() - lastTimeLoki > INTERVAL) //last time submited to loki + interval < current time
  {
    Serial.print(percentageHumididy);
    Serial.println("%");

    lastTimeLoki = millis();
    submitToLoki(percentageHumididy, 0);
  }
  delay(1000);
  }

void submitToLoki(float hum1, float hum2)
{
      // Get current timestamp
  unsigned long ts = getTime();

  String influxClient = String("https://") + INFLUX_HOST + "/api/v2/write?org=" + INFLUX_ORG_ID + "&bucket=" + INFLUX_BUCKET + "&precision=s";
  String body = String("humidity,plant=ananas value=" + String(hum1) + " " + ts + " \nhumidity,plant=blatt value=" + String(hum2) + " " + ts);

  // Submit POST request via HTTP
  httpInflux.begin(influxClient);
  httpInflux.addHeader("Authorization", INFLUX_TOKEN);
  int httpCode = httpInflux.POST(body);
  Serial.printf(INFLUX_TOKEN);
  Serial.printf("Influx [HTTPS] POST...  Code: %d\n", httpCode);

  httpInflux.end();
}


void startWatering () {
  startedWatering = millis();
  watering = true;
  digitalWrite(RELAY_PIN, HIGH);
}

void stopWatering () {
  watering = false;
  digitalWrite(RELAY_PIN, LOW);
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}
