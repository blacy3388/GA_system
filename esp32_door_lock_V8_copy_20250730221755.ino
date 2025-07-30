#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include "secrets.h"

// Pins for RC522 (SPI)
#define RST_PIN    4
#define SS_PIN     5

// Servo pin
#define SERVO_PIN  14

// LCD I2C address
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Objects
WiFiClient espClient;
PubSubClient client(espClient);
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myServo;

// --- Wi-Fi Connect ---
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// --- MQTT Reconnect ---
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-" + String(WiFi.macAddress());
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      client.subscribe(MQTT_TOPIC_SERVO);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

// --- MQTT Callback ---
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  if (String(topic) == MQTT_TOPIC_SERVO) {
    if (msg == "open") {
      myServo.write(90);
      lcd.clear(); lcd.print("Unlocked");
    } else if (msg == "close") {
      myServo.write(0);
      lcd.clear(); lcd.print("Locked");
    }
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin();
  mfrc522.PCD_Init();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");

  myServo.attach(SERVO_PIN);
  myServo.write(0); // Locked

  connectWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

// --- Loop ---
void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String tag = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      tag += String(mfrc522.uid.uidByte[i], HEX);
    }

    Serial.println("Tag: " + tag);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("UID:");
    lcd.setCursor(0, 1);
    lcd.print(tag);

    client.publish(MQTT_TOPIC_PUB, tag.c_str());

    myServo.write(90);  // Unlock
    delay(3000);
    myServo.write(0);   // Lock

    mfrc522.PICC_HaltA();
  }
}
