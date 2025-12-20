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
#define TOPIC_VOLTAGE "sensor/ina219/voltage"
#define TOPIC_CURRENT "sensor/ina219/current"
#define TOPIC_POWER   "sensor/ina219/power"

// I2C Pins
#define SDA_PIN 4 // D2
#define SCL_PIN 5 // D1

// OTA Passwort
#define OTA_PASSWORD "Insert your OTA password"

#endif
