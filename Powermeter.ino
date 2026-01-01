#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA228.h>
#include <ArduinoOTA.h>
#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_INA228 ina228;

unsigned long lastSend = 0;
const long interval = 60000; // 1 Minute

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

  // INA228 auslesen und senden
  if (millis() - lastSend > interval) {
    lastSend = millis();

    float shuntVoltage = ina228.getShuntVoltage_mV();
    float current_mA = ina228.getCurrent_mA();
    float power_mW   = shuntVoltage * current_mA;

    Serial.print("V: "); Serial.print(shuntVoltage); Serial.print(" V, ");
    Serial.print("I: "); Serial.print(current_mA); Serial.print(" mA, ");
    Serial.print("P: "); Serial.println(power_mW);

    client.publish(TOPIC_VOLTAGE, String(shuntVoltage,3).c_str(), true);
    client.publish(TOPIC_CURRENT, String(current_mA,3).c_str(), true);
    client.publish(TOPIC_POWER,   String(power_mW,3).c_str(), true);
  }
}
