#include <FirebaseESP8266.h>
#include <ESP8266WiFi.h>
#include <HCSR04.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "CTBot.h"

#define FIREBASE_HOST "smarttrashbinx-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "yUYJef7QEi6H28auOGvK2CIsfLQX6WkTg03shXHj"
#define WIFI_SSID "Chlorine"
#define WIFI_PASSWORD "mantub48"
FirebaseData firebaseData;
FirebaseJson json;

//initialize Ultrasonic Sensor
#define trig 13
#define echo 12
HCSR04 hc(trig, echo);
int capacity;
int oneTime;    //this variable is to limit the message sent to telegram

//initialize OLED Display 128x64
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);

//initialize GPS Module
float latitude;
float longitude;
const int RXPin = 0, TXPin = 2;
SoftwareSerial neo6m(RXPin, TXPin);
TinyGPSPlus gps;

//initialize Telegram Bot
CTBot myBot ;
TBMessage msg ;
String token = "5733588857:AAE2pphvOdDISDOdW0fl8AAV4auN52eZUro";
const int id = 999861828;

//initialize interval
unsigned long previousMillis = 0;   //for ultrasonic sensor
const long interval = 1000;
unsigned long previousMillis2 = 0;  //for oled display and telegram message
const long interval2 = 2000;
unsigned long previousMillis3 = 0;  //for gps module
const long interval3 = 5000;

void setup() {
  Serial.begin(115200);
  neo6m.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  myBot.setTelegramToken(token);
  pinMode(LED_BUILTIN, OUTPUT);
  
  //connecting to wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan...");
  
  //while wifi isn't connected
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  //when it's connected
  Serial.println();
  Serial.print("Terhubung ");
  Serial.println(WiFi.localIP());

  //connect to firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

void oledAndTelegram() {
  display.setCursor(18, 0);
  display.setTextSize(1);
  display.print("KAPASITAS SAMPAH");
  display.setCursor(40, 20);
  display.setTextSize(3);
  display.print(String(capacity) + "%");
  if (capacity < 70) {
    oneTime = 1;
    display.setCursor(30, 50);
    display.setTextSize(1);
    display.print("NOT FULL YET");
  } else if (capacity >= 70 && oneTime == 1) {
    display.setCursor(33, 50);
    display.setTextSize(1);
    display.print("ALMOST FULL");
    sendTelegram();
    oneTime = 2;
  } else if (capacity >= 80 && oneTime == 2) {
    display.setCursor(50, 50);
    display.setTextSize(1);
    display.print("FULL");
    sendTelegram();
    oneTime = 3;
  } else if (capacity >= 90 && oneTime == 3) {
    display.setCursor(33, 50);
    display.setTextSize(1);
    display.print("ALREADY FULL");
    sendTelegram();
    oneTime = 4;
  }
  display.display();
  display.clearDisplay();

  if (myBot.getNewMessage(msg)) {
    Serial.println("Pesan Masuk : " + msg.text);
    String pesan = msg.text;
    if (pesan == "check") {
      myBot.sendMessage(msg.sender.id, "Kapasitas capacity terisi: " + String(capacity) + "%" + "\nLokasi Tempat Sampah:" + "\nhttp://www.google.com/maps/place/" + String(latitude, 5) + "," + String(longitude, 5));
    }
  }
}

void sendTelegram() {
  myBot.sendMessage(id, "capacity Hampir Penuh " + String(capacity) + "% Sudah Terisi");
}

void ultrasonic() {
  capacity = map(hc.dist(), 23, 2, 0, 100);
  if (capacity < 5 || capacity > 100) {
    capacity = 0;
  }

  if (Firebase.setInt(firebaseData, "/GPS/usonic", capacity)) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("FAILED");
    Serial.println("REASON: " + firebaseData.errorReason());
    ESP.restart();
  }
}

void ubloxGPS() {
  smartdelay_gps(1000);

  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();

    if (Firebase.setFloat(firebaseData, "/GPS/f_latitude", latitude)) {
      Serial.println("OK");
    } else {
      Serial.println("REASON: " + firebaseData.errorReason());
      ESP.restart();
    }

    if (Firebase.setFloat(firebaseData, "/GPS/f_longitude", longitude)) {
      Serial.println("OK");
    } else {
      Serial.println("REASON: " + firebaseData.errorReason());
      ESP.restart();
    }

  } else {
    Serial.println("No valid GPS data found.");
  }
}

static void smartdelay_gps(unsigned long ms) {
  unsigned long start = millis();
  do
  {
    while (neo6m.available())
      gps.encode(neo6m.read());
  } while (millis() - start < ms);
}

void firebaseReconnect() {
  Serial.println("Trying to reconnect");
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ultrasonic();
  }

  if (currentMillis - previousMillis2 >= interval2) {
    previousMillis2 = currentMillis;
    oledAndTelegram();
  }

  if (currentMillis - previousMillis3 >= interval3) {
    previousMillis3 = currentMillis;
    ubloxGPS();
  }
}
