extern "C" {
  #include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA228.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <math.h>
#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_INA228 ina228;

unsigned long lastSend = 0;
const long interval = 60000; // 1 Minute
const char* NTP_SERVER = "pool.ntp.org"; // Time server

// Sonnenaufgang und Sonnenuntergang
double sunrise = 0;
double sunset  = 0;

/* =========================
   WLAN-Verbindung
   ========================= */
void connectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Verbinde mit WLAN ");
  Serial.println(WIFI_SSID);
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.macAddress());

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retries++;
    if (retries > 60) { // 30 Sekunden Timeout
      Serial.println("\nWLAN-Verbindung fehlgeschlagen, Neustart...");
      ESP.restart();
    }
  }

  Serial.println("\nWLAN verbunden!");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());
}

/* =========================
   MQTT-Verbindung
   ========================= */
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Verbinde mit MQTT...");
    String clientId = "ESP8266-INA228-";
    clientId += String(ESP.getChipId(), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("verbunden");
    } else {
      Serial.print("Fehler, rc=");
      Serial.print(client.state());
      Serial.println(" -> neuer Versuch in 5s");
      delay(5000);
    }
  }
}

/* =========================
   Julianisches Datum
   ========================= */
double julianDay(int year, int month, int day) {
  if (month <= 2) {
    year--;
    month += 12;
  }
  int A = year / 100;
  int B = 2 - A + A / 4;
  return floor(365.25 * (year + 4716))
       + floor(30.6001 * (month + 1))
       + day - 1524.5 + B;
}

/* =========================
   Sonnenauf-/untergang UTC
   ========================= */
double calcSunTimeUTC(bool sunrise, int year, int month, int day) {
  double JD = julianDay(year, month, day);
  double n = JD - 2451545.0 + 0.0008;
  double Jstar = n - (LONGITUDE / 360.0);

  double M = fmod(357.5291 + 0.98560028 * Jstar, 360.0);

  double C = 1.9148 * sin(M * DEG_TO_RAD)
           + 0.0200 * sin(2 * M * DEG_TO_RAD)
           + 0.0003 * sin(3 * M * DEG_TO_RAD);

  double lambda = fmod(M + C + 180 + 102.9372, 360.0);

  double Jtransit = 2451545.0 + Jstar
                  + 0.0053 * sin(M * DEG_TO_RAD)
                  - 0.0069 * sin(2 * lambda * DEG_TO_RAD);

  double delta = asin(sin(lambda * DEG_TO_RAD) * sin(23.44 * DEG_TO_RAD));

  double H = acos(
    (sin(-0.833 * DEG_TO_RAD)
    - sin(LATITUDE * DEG_TO_RAD) * sin(delta)) /
    (cos(LATITUDE * DEG_TO_RAD) * cos(delta))
  );

  double J = sunrise
             ? Jtransit - H / (2 * PI)
             : Jtransit + H / (2 * PI);

  return fmod((J - floor(J)) * 24.0 + 24.0, 24.0);
}

/* =========================
   NTP-Zeit Setup
   ========================= */
void setupTimeUTC() {
  configTime(0, 0, NTP_SERVER);  // UTC, keine Offsets

  struct tm timeinfo;
  Serial.print("Warte auf NTP-Zeit");

  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK");
}

/* =========================
   Berechne Sonnenaufgang und Sonnenuntergang
   ========================= */
void calcSunriseAndSunset() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  int year  = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon + 1;
  int day   = timeinfo.tm_mday;

  sunrise = calcSunTimeUTC(true, year, month, day);
  sunset  = calcSunTimeUTC(false, year, month, day);
}

/* =========================
   Ausgabe Zeit
   ========================= */
String timeToString(double hours) {
  int h = (int)hours;
  int m = (int)((hours - h) * 60);

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d:%02d UTC", h, m);

  return String(buffer);
}

/* =========================
   Setup
   ========================= */
void setup() {
  Serial.begin(115200);
  delay(100);

  // I2C starten
  Wire.begin(SDA_PIN, SCL_PIN);

  // INA228 initialisieren und kalibrieren
  if (!ina228.begin()) {
    Serial.println("INA228 nicht gefunden!");
    while (1) { delay(10); }
  }
  
  // Shunt = 15 mΩ, max_current = 3.5 A (Puffer über 3A)
  ina228.setShunt(0.015, 3.5);

  ina228.setAveragingCount(INA228_COUNT_64);

  // set the time over which to measure the current and bus voltage
  ina228.setVoltageConversionTime(INA228_TIME_1052_us);
  ina228.setCurrentConversionTime(INA228_TIME_1052_us);

  Serial.println("INA228 bereit");

  // WLAN verbinden
  connectWiFi();

  // MQTT einrichten
  client.setServer(MQTT_SERVER, MQTT_PORT);

  // Zeit holen
  setupTimeUTC();

  // Light Sleep
  wifi_set_sleep_type(LIGHT_SLEEP_T);

  // OTA einrichten
  ArduinoOTA.setHostname("ESP8266-Powermeter");
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "Sketch" : "Filesystem";
    Serial.println("OTA Start: " + type);
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA Ende"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Fortschritt: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Fehler");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Fehler");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Fehler");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Fehler");
    else if (error == OTA_END_ERROR) Serial.println("End Fehler");
  });
  ArduinoOTA.begin();
  Serial.println("OTA bereit!");
}

/* =========================
   Loop
   ========================= */
void loop() {
  ArduinoOTA.handle();  // OTA-Update prüfen

  // WLAN prüfen
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WLAN getrennt, versuche erneut zu verbinden...");
    connectWiFi();
  }

  // MQTT prüfen
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  if (millis() - lastSend > interval) {
    lastSend = millis();

    // Berechne Sonnenaufgang und Sonnenuntergang
    calcSunriseAndSunset();
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int sunriseMinutes = (int)(sunrise * 60);
    int sunsetMinutes  = (int)(sunset * 60);
    if (nowMinutes < sunriseMinutes || nowMinutes >= sunsetMinutes) {
      return; // Nacht -> Code NICHT ausführen
    }
    // Hier scheint die Sonne

    // INA228 auslesen und senden
    float shuntVoltage = ina228.getShuntVoltage_mV();
    float current_mA = ina228.getCurrent_mA();
    float power_mW   = shuntVoltage * current_mA;

    Serial.print("V: "); Serial.print(shuntVoltage); Serial.print(" V, ");
    Serial.print("I: "); Serial.print(current_mA); Serial.print(" mA, ");
    Serial.print("P: "); Serial.println(power_mW);

    client.publish(TOPIC_VOLTAGE, String(shuntVoltage,3).c_str(), true);
    client.publish(TOPIC_CURRENT, String(current_mA,3).c_str(), true);
    client.publish(TOPIC_POWER,   String(power_mW,3).c_str(), true);
    client.publish(TOPIC_SUNRISE, String(timeToString(sunrise)).c_str(), true);
    client.publish(TOPIC_SUNSET,  String(timeToString(sunset)).c_str(), true);
  }
}
