#ifndef CONFIG_H
#define CONFIG_H

// WLAN-Zugang
#define WIFI_SSID "Insert your SSID"
#define WIFI_PASS "Insert your password"

// MQTT-Zugang
#define MQTT_SERVER "Insert your server ip"
#define MQTT_PORT   1883
#define MQTT_USER   "Insert your MQTT user"   // optional, falls kein User: ""
#define MQTT_PASS   "Insert your MQTT password"   // optional, falls kein Passwort: ""

// MQTT Topics
#define TOPIC_VOLTAGE "sensor/ina228/voltage"
#define TOPIC_CURRENT "sensor/ina228/current"
#define TOPIC_POWER   "sensor/ina228/power"
#define TOPIC_SUNRISE "sensor/ina228/sunrise"
#define TOPIC_SUNSET  "sensor/ina228/sunset"

// I2C Pins
#define SDA_PIN 4 // D2
#define SCL_PIN 5 // D1

// OTA Passwort
#define OTA_PASSWORD "Insert your OTA password"

// Location
#define LATITUDE   50.0000
#define LONGITUDE  13.0000

#endif
