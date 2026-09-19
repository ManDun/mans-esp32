#include "secrets.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* mqtt_server = MQTT_SERVER;
const int   mqtt_port   = MQTT_PORT;
const char* mqtt_user   = MQTT_USER;
const char* mqtt_pass   = MQTT_PASS;
const char* mqtt_client_id = "esp32-weather";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---- NTP ----
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 36000;
const int   daylightOffset_sec = 0;

// ---- OLED ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---- DHT ----
#define DHTPIN 19
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---- LDR ----
#define LDR_PIN 34

float latestTemp = NAN;
float latestHum  = NAN;
int   latestLight = 0;
char  latestTimeStr[16] = "";
char  latestDateStr[16] = "";

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      mqttClient.publish("homeassistant/sensor/esp32_temp/config",
        "{\"name\":\"ESP32 Temperature\",\"state_topic\":\"esp32/weather/temperature\",\"unit_of_measurement\":\"\u00b0C\",\"device_class\":\"temperature\",\"unique_id\":\"esp32_temp_1\"}", true);
      mqttClient.publish("homeassistant/sensor/esp32_hum/config",
        "{\"name\":\"ESP32 Humidity\",\"state_topic\":\"esp32/weather/humidity\",\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\",\"unique_id\":\"esp32_hum_1\"}", true);
      mqttClient.publish("homeassistant/sensor/esp32_light/config",
        "{\"name\":\"ESP32 Light Level\",\"state_topic\":\"esp32/weather/light\",\"unique_id\":\"esp32_light_1\",\"icon\":\"mdi:brightness-6\"}", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (true);
  }
  display.clearDisplay();
  display.display();
  display.setTextColor(SSD1306_WHITE);

  dht.begin();
  delay(1500);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    mqttClient.setServer(mqtt_server, mqtt_port);
    connectMQTT();
  }
}

void loop() {
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  struct tm timeinfo;
  bool timeOk = getLocalTime(&timeinfo, 1000);
  if (timeOk) {
    strftime(latestDateStr, sizeof(latestDateStr), "%d %b %Y", &timeinfo);
    strftime(latestTimeStr, sizeof(latestTimeStr), "%H:%M:%S", &timeinfo);
  }

  latestTemp  = dht.readTemperature();
  latestHum   = dht.readHumidity();
  latestLight = analogRead(LDR_PIN); // 0-4095

  if (!isnan(latestTemp) && !isnan(latestHum)) {
    char tempPayload[8];
    char humPayload[8];
    char lightPayload[8];
    dtostrf(latestTemp, 4, 1, tempPayload);
    dtostrf(latestHum, 4, 1, humPayload);
    itoa(latestLight, lightPayload, 10);
    mqttClient.publish("esp32/weather/temperature", tempPayload);
    mqttClient.publish("esp32/weather/humidity", humPayload);
    mqttClient.publish("esp32/weather/light", lightPayload);
  }

  display.clearDisplay();
  display.setTextSize(1);

  // Row 1: time + date
  display.setCursor(0, 0);
  if (timeOk) {
    display.print(latestTimeStr);
    display.print("  ");
    display.print(latestDateStr);
  } else {
    display.print("Time unavailable");
  }

  // Row 2 & 3: temp / humidity
  if (isnan(latestTemp) || isnan(latestHum)) {
    display.setCursor(0, 8);
    display.println("Sensor read failed");
  } else {
    display.setCursor(0, 8);
    display.print("Temp:     ");
    display.print(latestTemp, 1);
    display.print(" C");

    display.setCursor(0, 16);
    display.print("Humidity: ");
    display.print(latestHum, 0);
    display.print(" %");
  }

  // Row 4: light level
  display.setCursor(0, 24);
  display.print("Light:    ");
  display.print(latestLight);

  display.display();
  delay(10000);
}