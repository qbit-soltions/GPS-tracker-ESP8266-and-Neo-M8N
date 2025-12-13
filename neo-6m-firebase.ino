#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// WiFi credentials
#define WIFI_SSID "vivo 1820"
#define WIFI_PASSWORD "12345678"

// Firebase configuration
#define FIREBASE_HOST "your firebase url"
#define FIREBASE_AUTH "your firebase authentication token"

// Define Firebase objects
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// GPS Module connections
static const int RXPin = D1, TXPin = D2;
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

unsigned long previousMillis = 0;
const long interval = 1000;
bool newDataAvailable = false;

void setup() {
  Serial.begin(115200);
  ss.begin(GPSBaud);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Initialize Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
}

void loop() {
  unsigned long currentMillis = millis();
  
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      newDataAvailable = true;
    }
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    if (newDataAvailable) {
      processAndSendGPSData();
      newDataAvailable = false;
    } else {
      // Send 0 values when no new valid data is availabl
      sendDefaultValues();
    }
  }

  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println("No GPS detected - check wiring");
  }
}

void processAndSendGPSData() {
  // Use current timestamp as the key instead of auto-generated push ID
  String path = "/gps_data/" + String(millis());
  FirebaseJson json;
  
  if (gps.location.isValid()) {
    // Convert double values to String with specified precision
    json.set("latitude", String(gps.location.lat(), 6));
    json.set("longitude", String(gps.location.lng(), 6));
    json.set("altitude", gps.altitude.isValid() ? String(gps.altitude.meters(), 2) : "0");
    json.set("speed", gps.speed.isValid() ? String(gps.speed.kmph(), 2) : "0");
    json.set("satellites", gps.satellites.isValid() ? gps.satellites.value() : 0);
    json.set("hdop", gps.hdop.isValid() ? String(gps.hdop.value()/100.0, 2) : "0");

    String timestamp = "";
    if (gps.date.isValid() && gps.time.isValid()) {
      timestamp = String(gps.date.year()) + "-" + 
                 String(gps.date.month()) + "-" + 
                 String(gps.date.day()) + " " +
                 String(gps.time.hour()) + ":" +
                 String(gps.time.minute()) + ":" +
                 String(gps.time.second());
    } else {
      timestamp = "0";
    }
    json.set("timestamp", timestamp);

    Serial.print("Lat: "); Serial.print(gps.location.lat(), 6);
    Serial.print(" Lng: "); Serial.print(gps.location.lng(), 6);
    Serial.print(" Alt: "); Serial.print(gps.altitude.isValid() ? gps.altitude.meters() : 0);
    Serial.print("m Speed: "); Serial.print(gps.speed.isValid() ? gps.speed.kmph() : 0);
    Serial.print("km/h Sats: "); Serial.println(gps.satellites.isValid() ? gps.satellites.value() : 0);
  } else {
    // If location is invalid, send all 0 values
    sendDefaultValues();
    return;
  }

  // Use setJSON instead of pushJSON to write directly to the specified path
  if (Firebase.setJSON(firebaseData, path, json)) {
    Serial.println("Data sent to Firebase");
  } else {
    Serial.println("Failed to send data");
    Serial.println("Reason: " + firebaseData.errorReason());
  }
}

void sendDefaultValues() {
  // Use current timestamp as the key
  String path = "/gps_data/" + String(millis());
  FirebaseJson json;
  
  json.set("latitude", "0");
  json.set("longitude", "0");
  json.set("altitude", "0");
  json.set("speed", "0");
  json.set("satellites", 0);
  json.set("hdop", "0");
  json.set("timestamp", "0");

  // Use setJSON instead of pushJSON
  if (Firebase.setJSON(firebaseData, path, json)) {
    Serial.println("Sent default (0) values to Firebase - No GPS fix");
  } else {
    Serial.println("Failed to send default values");
    Serial.println("Reason: " + firebaseData.errorReason());
  }
}