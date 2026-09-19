#ifndef CONFIG_H
#define CONFIG_H

// MQTT
#define MQTT_CLIENT_ID "esp32-weather"

#define MQTT_TOPIC_TEMPERATURE "esp32/weather/temperature"
#define MQTT_TOPIC_HUMIDITY    "esp32/weather/humidity"
#define MQTT_TOPIC_LIGHT       "esp32/weather/light"

#define HA_DISCOVERY_TOPIC_TEMP  "homeassistant/sensor/esp32_temp/config"
#define HA_DISCOVERY_TOPIC_HUM   "homeassistant/sensor/esp32_hum/config"
#define HA_DISCOVERY_TOPIC_LIGHT "homeassistant/sensor/esp32_light/config"

// NTP
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 36000
#define DAYLIGHT_OFFSET_SEC 0

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// DHT
#define DHT_PIN 19
#define DHT_TYPE DHT11

// LDR
#define LDR_PIN 34

// RGB status LED
#define LED_RED_PIN   18
#define LED_GREEN_PIN 5
#define LED_BLUE_PIN  15
#define LED_BLINK_INTERVAL_MS 500

// Timing
#define SENSOR_PUBLISH_INTERVAL_MS 10000

#endif
