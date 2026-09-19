#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "secrets.h"
#include "config.h"

// ---- RGB LED ----
enum LedState { LED_CONNECTING, LED_OK, LED_ERROR };
LedState currentLedState = LED_CONNECTING;
unsigned long lastBlinkTime = 0;
bool ledBlinkOn = false;
const unsigned long BLINK_INTERVAL = LED_BLINK_INTERVAL_MS;

void setLedColor(bool r, bool g, bool b) {
  digitalWrite(LED_RED_PIN, r ? HIGH : LOW);
  digitalWrite(LED_GREEN_PIN, g ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN, b ? HIGH : LOW);
}

void updateStatusLed() {
  if (currentLedState == LED_OK) {
    setLedColor(false, true, false); // solid green, no blink
    return;
  }

  unsigned long now = millis();
  if (now - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = now;
    ledBlinkOn = !ledBlinkOn;
  }

  if (!ledBlinkOn) {
    setLedColor(false, false, false); // off phase of blink
    return;
  }

  switch (currentLedState) {
    case LED_CONNECTING: setLedColor(false, false, true);  break; // blue
    case LED_ERROR:       setLedColor(true, false, false); break; // red
    default: break;
  }
}

// ---- WiFi ----
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// ---- MQTT ----
const char* mqtt_server = MQTT_SERVER;
const int   mqtt_port   = MQTT_PORT;
const char* mqtt_user   = MQTT_USER;
const char* mqtt_pass   = MQTT_PASS;
const char* mqtt_client_id = MQTT_CLIENT_ID;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---- NTP ----
const char* ntpServer = NTP_SERVER;
const long  gmtOffset_sec = GMT_OFFSET_SEC;
const int   daylightOffset_sec = DAYLIGHT_OFFSET_SEC;

// ---- OLED ----
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---- DHT ----
DHT dht(DHT_PIN, DHT_TYPE);

float latestTemp = NAN;
float latestHum  = NAN;
int   latestLight = 0;
char  latestTimeStr[16] = "";
char  latestDateStr[16] = "";
bool  sensorError = false;

void connectMQTT() {
  while (!mqttClient.connected()) {
    currentLedState = LED_CONNECTING;
    updateStatusLed();

    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_pass)) {
      mqttClient.publish(HA_DISCOVERY_TOPIC_TEMP,
        "{\"name\":\"ESP32 Temperature\",\"state_topic\":\"" MQTT_TOPIC_TEMPERATURE "\",\"unit_of_measurement\":\"\u00b0C\",\"device_class\":\"temperature\",\"unique_id\":\"esp32_temp_1\"}", true);
      mqttClient.publish(HA_DISCOVERY_TOPIC_HUM,
        "{\"name\":\"ESP32 Humidity\",\"state_topic\":\"" MQTT_TOPIC_HUMIDITY "\",\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\",\"unique_id\":\"esp32_hum_1\"}", true);
      mqttClient.publish(HA_DISCOVERY_TOPIC_LIGHT,
        "{\"name\":\"ESP32 Light Level\",\"state_topic\":\"" MQTT_TOPIC_LIGHT "\",\"unique_id\":\"esp32_light_1\",\"icon\":\"mdi:brightness-6\"}", true);
    } else {
      currentLedState = LED_ERROR;
      updateStatusLed();
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  currentLedState = LED_CONNECTING;

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
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
    updateStatusLed();
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    mqttClient.setServer(mqtt_server, mqtt_port);
    connectMQTT();
    currentLedState = LED_OK;
  } else {
    currentLedState = LED_ERROR;
  }
}

void loop() {
  updateStatusLed();

  if (!mqttClient.connected()) {
    currentLedState = LED_CONNECTING;
    connectMQTT();
  }
  mqttClient.loop();

  struct tm timeinfo;
  bool timeOk = getLocalTime(&timeinfo, 1000);
  if (timeOk) {
    strftime(latestDateStr, sizeof(latestDateStr), "%d %b %Y", &timeinfo);
    strftime(latestTimeStr, sizeof(latestTimeStr), "%H:%M:%S", &timeinfo);
  }

  latestTemp  = dht.readTemperature();
  latestHum   = dht.readHumidity();
  latestLight = analogRead(LDR_PIN);

  sensorError = isnan(latestTemp) || isnan(latestHum) || !timeOk;

  if (WiFi.status() == WL_CONNECTED && mqttClient.connected() && !sensorError) {
    currentLedState = LED_OK;
  } else if (sensorError) {
    currentLedState = LED_ERROR;
  }

  if (!isnan(latestTemp) && !isnan(latestHum)) {
    char tempPayload[8], humPayload[8], lightPayload[8];
    dtostrf(latestTemp, 4, 1, tempPayload);
    dtostrf(latestHum, 4, 1, humPayload);
    itoa(latestLight, lightPayload, 10);
    mqttClient.publish(MQTT_TOPIC_TEMPERATURE, tempPayload);
    mqttClient.publish(MQTT_TOPIC_HUMIDITY, humPayload);
    mqttClient.publish(MQTT_TOPIC_LIGHT, lightPayload);
  }

  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0);
  if (timeOk) {
    display.print(latestTimeStr);
    display.print("  ");
    display.print(latestDateStr);
  } else {
    display.print("Time unavailable");
  }

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

  display.setCursor(0, 24);
  display.print("Light:    ");
  display.print(latestLight);

  display.display();

  unsigned long cycleStart = millis();
  while (millis() - cycleStart < SENSOR_PUBLISH_INTERVAL_MS) {
    updateStatusLed();
    if (!mqttClient.connected()) {
      currentLedState = LED_CONNECTING;
      break; // exit early so MQTT can reconnect promptly
    }
    delay(50);
  }
}