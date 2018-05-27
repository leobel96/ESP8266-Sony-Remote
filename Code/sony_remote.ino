//----------------------------------------------------------------------------------------------------------------------
// This program is based on: WiFiClient from ESP libraries
//
// Camera handling by Reinhard Nickels https://glaskugelsehen.wordpress.com/
// tested with DSC-HX90V, more about protocol in documentation of CameraRemoteAPI https://developer.sony.com/develop/cameras/
// 
// Licenced under the Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0) licence:
// http://creativecommons.org/licenses/by-sa/3.0/
//
// Requires Arduino IDE with esp8266 core: https://github.com/esp8266/Arduino install by boardmanager
//----------------------------------------------------------------------------------------------------------------------

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "Functions.h"

#define DEBUG
//#define LED //Define if you want a led that switches off when battery is low
#define BEEP //Define if you want camera's sounds switch on when battery is low
#define PHOTOBUTTON D5  //D5 pin
#define VIDEOBUTTON D2  //D2 pin
#define DEBOUNCETIME 2000 //This prevents debounce problems
#define BATTERY //Define if battery is present
#define BATTERYLOW 3.4  //Battery limit in Volt
#define WAITTIME 1500
#ifdef LED
  #define LEDPIN D4 //LED pin
#endif

//----------------------------------------------------------------------------------------------------------------------
//Sony action camera configuration
const char* ssid     = "HERE";  //Your cam's SSID
const char* password = "HERE";  //Your cam's WPA2 password
const char* host = "HERE"; //Fixed IP of camera
const int httpPort = HERE; //Default port
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
//HTTP requests definition
char getInfo[] = "{\"method\": \"getVersions\",\"params\": [],\"id\": 1,\"version\": \"1.0\"}";
char videoMode[] = "{\"method\": \"setShootMode\",\"params\": [\"movie\"],\"id\": 1,\"version\": \"1.0\"}";
char startRec[] = "{\"method\": \"startMovieRec\",\"params\": [],\"id\": 1,\"version\": \"1.0\"}";
char stopRec[] = "{\"method\": \"stopMovieRec\",\"params\": [],\"id\": 1,\"version\": \"1.0\"}";
char photoMode[] = "{\"method\": \"setShootMode\",\"params\": [\"still\"],\"id\": 1,\"version\": \"1.0\"}";
char takePhoto[] = "{\"method\": \"actTakePicture\",\"params\": [],\"id\": 1,\"version\": \"1.0\"}"; 
char beepOn[] = "{\"method\": \"setBeepMode\",\"params\": [\"On\"],\"id\": 1,\"version\": \"1.0\"}";
char beepOff[] = "{\"method\": \"setBeepMode\",\"params\": [\"Off\"],\"id\": 1,\"version\": \"1.0\"}";
//----------------------------------------------------------------------------------------------------------------------

unsigned long timeVideo = 500;  //Set to 500 to speed up initialization
unsigned long timePhoto = 500;
bool videoStatus = false; //True if it's recording
volatile bool videoFlag = false; //Flag for video interrupt
volatile bool photoFlag = false; //FLag for photo interrupt
bool readyToReceive = false;  //True when initialized
 
WiFiClient client;
 
void setup() {
  Serial.begin(115200);
  delay(10);
  
  setupIO();  //Pins configuration
  setupWifi();  //Connects to cam's WiFi
  
  #ifdef BATTERY
    float voltage = analogRead(A0) * 4.2 / 1023;
    Serial.print("voltage = ");
    Serial.println(voltage);
    Serial.println(analogRead(A0));
    if(voltage <= BATTERYLOW){
      #ifdef BEEP
        httpPost(beepOn);
      #endif
      #ifdef LED
        digitalWrite(LEDPIN, LOW);
      #endif
    }else{
      #ifdef BEEP
        httpPost(beepOff);
      #endif
      #ifdef LED
        digitalWrite(LEDPIN, HIGH);
      #endif
    }  
  #endif  
}
 
void loop() {
  if (photoFlag){
    #ifdef DEBUG
      Serial.print("Setting photo mode...");
    #endif
    httpPost(photoMode);
    #ifdef DEBUG
      Serial.println("photo mode set");
    #endif
    delay(50);
    #ifdef DEBUG
      Serial.print("Taking photo...");
    #endif
    httpPost(takePhoto);  //actTakePicture
    photoFlag = false;
    Serial.println("photo taken");
  }else if (videoFlag){    
    if (videoStatus){
      #ifdef DEBUG
        Serial.print("Stopping registration...");
      #endif
      httpPost(stopRec);
      videoStatus = false;
      #ifdef DEBUG1
        Serial.println("registration stopped");
      #endif
    }else{
      #ifdef DEBUG
        Serial.print("Setting video mode...");
      #endif
      httpPost(videoMode);
      #ifdef DEBUG
        Serial.println("video mode set");
      #endif
      delay(50);
      #ifdef DEBUG
        Serial.print("Starting registration...");
      #endif
      httpPost(startRec);
      videoStatus = true;
      Serial.print("registration started");
    }
    videoFlag = false; 
  }
}

void setupIO(){
  #ifdef LED
    pinMode(LEDPIN, OUTPUT);
  #endif
  pinMode(PHOTOBUTTON, INPUT_PULLUP);
  pinMode(VIDEOBUTTON, INPUT_PULLUP);
  pinMode(A0, INPUT);
  attachInterrupt(digitalPinToInterrupt(PHOTOBUTTON), photoButtonCode, FALLING);
  attachInterrupt(digitalPinToInterrupt(VIDEOBUTTON), videoButtonCode, FALLING);
}

void setupWifi(){
  #ifdef DEBUG
    Serial.print("Connecting to ");
    Serial.println(ssid);
  #endif
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {   // wait for WiFi connection
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  readyToReceive = true;  //Now you can press buttons
}

void httpPost(char* jString) {
  #ifdef DEBUG
    Serial.print("Msg send: ");
    Serial.println(jString);
    Serial.print("Connecting to ");
    Serial.println(host);
  #endif
  if (!client.connect(host, httpPort)) {
    Serial.println("HTTP connection failed");
    return;
  }
  else {
    Serial.print("connected to ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(httpPort);
  }
 
  String url = "/sony/camera";  //URI for the request
  #ifdef DEBUG
    Serial.print("Requesting URL: ");
    Serial.println(url);
  #endif
  // This will send the request to the server
  client.print(String("POST " + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n"));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(strlen(jString));
  client.println(); //End of headers
  client.println(jString);  // Request body
  #ifdef DEBUG
    Serial.println("wait for data");
  #endif
  int lastmillis = millis();
  while (!client.available() && millis() - lastmillis < 8000) {} // wait 8s max for answer
 
  #ifdef DEBUG
    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println();
    Serial.println("----closing connection----");
    Serial.println();
  #endif
  delay(WAITTIME);
  client.stop();
}

void photoButtonCode(){
  if(readyToReceive && ((millis() - timePhoto) > DEBOUNCETIME)){
    timePhoto = millis();
    photoFlag = true;
  }
}

void videoButtonCode(){
  if(readyToReceive && ((millis() - timeVideo) > DEBOUNCETIME)){ 
    timeVideo = millis();
    videoFlag = true;
  }
}
