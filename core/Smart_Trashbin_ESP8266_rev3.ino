#include <FirebaseESP8266.h>
#include <WiFiManager.h>
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
FirebaseData firebaseData;
FirebaseJson json;

//initialize Ultrasonic Sensor
#define trig D5
#define echo D6
HCSR04 hc(trig, echo);
int capacity;
int oneTime;    //this variable is to limit the message sent to telegram

//initialize OLED Display 128x64
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);

//initialize GPS Module
float latitude;
float longitude;
const int RXPin = 13, TXPin = 0;
SoftwareSerial neo6m(RXPin, TXPin);
TinyGPSPlus gps;

//initialize Telegram Bot
CTBot myBot ;
TBMessage msg ;
String token = "5733588857:AAE2pphvOdDISDOdW0fl8AAV4auN52eZUro";
const int id = 999861828;

//initialize interval
unsigned long previousMillis = 0;   //for ultrasonic sensor
const long interval = 10000;
unsigned long previousMillis2 = 0;  //for oled display and telegram message
const long interval2 = 20000;
unsigned long previousMillis3 = 0;  //for gps module
const long interval3 = 60000;

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  starting();
  neo6m.begin(9600);
  WiFiManager wm;
  bool res;
  res = wm.autoConnect();
  if (!res) {
    Serial.println("Failed to connect");
  }
  else {
    Serial.println("Connected to:");
    Serial.print(wm.getWiFiSSID());
    display.clearDisplay();
    display.setCursor(30, 5);
    display.setTextSize(1);
    display.print("CONNECTED TO");
    display.setCursor(25, 27);
    display.setTextSize(2);
    display.print(wm.getWiFiSSID());
    display.display();
    delay(1500);
  }
  myBot.setTelegramToken(token);
  pinMode(LED_BUILTIN, OUTPUT);

  //connect to firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

void oledAndTelegram() {
  display.clearDisplay();
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
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("FAILED");
    Serial.println("REASON: " + firebaseData.errorReason());
    starting();
    firebaseReconnect();
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
      starting();
      firebaseReconnect();
    }

    if (Firebase.setFloat(firebaseData, "/GPS/f_longitude", longitude)) {
      Serial.println("OK");
    } else {
      Serial.println("REASON: " + firebaseData.errorReason());
      starting();
      firebaseReconnect();
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

void starting() {
  display.clearDisplay();
  display.setCursor(21, 5);
  display.setTextSize(1);
  display.print("WAITING FOR");
  display.setCursor(0, 32);
  display.setTextSize(2);
  display.print("INTERNET CONNECTION");
  display.display();
}
void loop() {
  unsigned long currentMillis = millis();
  if (myBot.getNewMessage(msg)) {
    Serial.println("Pesan Masuk : " + msg.text);
    String pesan = msg.text;
    if (pesan == "/check") {
      myBot.sendMessage(msg.sender.id, "Kapasitas capacity terisi: " + String(capacity) + "%" + "\nLokasi Tempat Sampah:" + "\nhttp://www.google.com/maps/place/" + String(latitude, 5) + "," + String(longitude, 5));
    }
    if (pesan == "/start") {
      String welcome = "Welcome, " + msg.sender.username + "\n";
      welcome += "Use the following commands to interact with the Smart Trash Bin \n";
      welcome += "/check : check the capacity of trash bin\n";
      myBot.sendMessage(msg.sender.id, welcome, "");
    }
  }
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
