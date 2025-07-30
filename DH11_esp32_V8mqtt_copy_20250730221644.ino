#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "secrets.h"

// === Pin Setup ===
#define DHTPIN    15
#define DHTTYPE   DHT11
#define PIRPIN    12
#define TFT_CS    5
#define TFT_DC    16  
#define TFT_RST   4


// === Objects ===
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// === Globals ===
unsigned long lastMsg = 0;
int motionCount = 0;
bool lastMotion = false;
float temperature = 0.0;
float humidity = 0.0;
String motionStatus = "NO";

// === MQTT Callback ===
void callback(char* topic, byte* payload, unsigned int length) {
  String command;
  for (int i = 0; i < length; i++) command += (char)payload[i];

  if (String(topic) == "home/esp32/reset" && command == "reset") {
    motionCount = 0;
    Serial.println("Motion count reset via MQTT");
    //tft.fillScreen(ILI9341_BLACK)
    tft.fillRect(20, 250, 300, 30, ILI9341_BLACK);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(20, 180);
    tft.setTextSize(2);
    tft.println("Motion count reset!");
  }
}

// === Wi-Fi Setup ===
void setup_wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println(WIFI_SSID);
  Serial.println(WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("disconnected");
    Serial.println(WiFi.status());
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

// === MQTT Reconnect ===
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Sensor", MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      client.subscribe("home/esp32/reset");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// === TFT Display Header ===
void drawHeader() {
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(3);
  tft.setCursor(20, 10);
  tft.println("Sensors");
  tft.drawFastHLine(10, 45, 350, ILI9341_DARKGREY);
}

// === TFT Data Display ===
void displayData() {
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);

  tft.setCursor(20, 60);  tft.print("Temp:");  tft.setCursor(120, 60); tft.printf("%.1f C", temperature);
  tft.setCursor(20, 90);  tft.print("Hum:");   tft.setCursor(120, 90); tft.printf("%.1f %%", humidity);
  tft.setCursor(20, 120); tft.print("Motion:");tft.setCursor(120, 120); tft.print(motionStatus);
  tft.setCursor(20, 150); tft.print("Count:"); tft.setCursor(120, 150); tft.print(motionCount);
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  pinMode(PIRPIN, INPUT);
  dht.begin();
  tft.begin();
  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);
  drawHeader();

  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

// === Main Loop ===
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    bool motion = digitalRead(PIRPIN);
    motionStatus = motion ? "YES" : "NO";

    if (motion && !lastMotion) motionCount++;
    lastMotion = motion;

    displayData();

    if (!isnan(temperature))
      client.publish("home/esp32/temperature", String(temperature).c_str(), true);
    if (!isnan(humidity))
      client.publish("home/esp32/humidity", String(humidity).c_str(), true);
    client.publish("home/esp32/motion", motion ? "ON" : "OFF", true);
    client.publish("home/esp32/motion_count", String(motionCount).c_str(), true);
  }
}
