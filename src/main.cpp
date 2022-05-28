#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h> 
#include "time.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define GREEN_LED 13
#define YELLOW_LED 12
#define RED_LED 14
#define GROVE_WATER_SENSOR 34
#define BUZZER_PIN 15


#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define USER_EMAIL ""
#define USER_PASSWORD ""

#define API_KEY ""
#define FIREBASE_ID_PROJECT ""


//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool greenLedState = false;
bool yellowLedState = false;
bool redLedState = false;

bool signupOK = false;
String timestamp;
String uid;
String databasePath;

const char* ntpServer = "pool.ntp.org";

LiquidCrystal lcd(25, 33, 32, 19, 18, 26);
String lastState = "";

void setEnvironment(){
  lcd.begin(16, 2);
  lcd.clear();

  lcd.setCursor(2, 0);
  lcd.print("Water Level: ");
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(GREEN_LED, greenLedState);

  pinMode(YELLOW_LED, OUTPUT);
  digitalWrite(YELLOW_LED, yellowLedState);

  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, redLedState);
}

void initWiFi(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  
  Serial.println();

  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void initFirebase(){
  config.api_key = API_KEY;
  // config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);


  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth); // set time out warning

  Serial.println("Getting User UID");
  while((auth.token.uid) == ""){
    Serial.print(".");
    delay(1000);
  }
  Serial.println();

  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  String wifiMacString = WiFi.macAddress();

  databasePath = "water_sensor/" + wifiMacString + "/readings";
}

String getState(int currentWaterLevel){
  if(currentWaterLevel < 1900) return "LOW";
  if(currentWaterLevel < 2200) return "MEDIUM";
  return "HIGH";
}

void updateStates(String currentState){
  if(currentState == "LOW"){
    greenLedState = true;
    yellowLedState = false;
    redLedState = false;

  }else if(currentState == "MEDIUM"){
    greenLedState = false;
    yellowLedState = true;
    redLedState = false;

  }else if(currentState == "HIGH"){
    greenLedState = false;
    yellowLedState = false;
    redLedState = true;

  }
  
  lcd.clear();

  lcd.setCursor(2, 0);
  lcd.print("Water Level: ");

  lcd.setCursor(6, 1);
  lcd.print(currentState);

  digitalWrite(GREEN_LED, greenLedState);
  digitalWrite(YELLOW_LED, yellowLedState);
  digitalWrite(RED_LED, redLedState);

  if(redLedState){
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

String getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return "";
  }
  time(&now);
  
  char buf[sizeof "2011-10-08T07:07:09Z"];
  strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
  return String(buf);
}

void setup() {
  Serial.begin(9600);
  setEnvironment();
  initWiFi();
  configTime(0, 0, ntpServer);

  initFirebase();
}

void loop() {
  int currentWaterLevel = analogRead(GROVE_WATER_SENSOR); 

  Serial.println("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");
  Serial.print("Water Level: ");
  Serial.println(currentWaterLevel, DEC);
  Serial.println("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=");

  String currentState = getState(currentWaterLevel);

  updateStates(currentState);

  if(currentState != lastState){
    lastState = currentState;

    if(Firebase.ready() && WiFi.status() == WL_CONNECTED){
      FirebaseJson content;
      timestamp = getTime();
      String parentPath = databasePath + "/" + String(timestamp);

      content.set("/fields/macAddress/stringValue", String(WiFi.macAddress()).c_str());
      content.set("/fields/waterLevel/integerValue", currentWaterLevel);
      content.set("/fields/state/stringValue", String(currentState).c_str());
      content.set("/fields/time/timestampValue", timestamp);
      
      if(Firebase.Firestore.createDocument(&fbdo, FIREBASE_ID_PROJECT, "", parentPath.c_str(), content.raw())){
        Serial.println(fbdo.payload().c_str());
       
      }else{
        Serial.println(fbdo.errorReason());
 
      }
  
      if(currentState == "HIGH"){
        String alertPath = "alerts/"+String(timestamp);
        if(Firebase.Firestore.createDocument(&fbdo, FIREBASE_ID_PROJECT, "", alertPath.c_str(), content.raw())){
          Serial.println(fbdo.payload().c_str());
    
        }else{
          Serial.println(fbdo.errorReason());
     
        }
      }
    
    }
  }


  delay(2000);
}

