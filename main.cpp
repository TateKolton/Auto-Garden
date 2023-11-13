#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <HTTPClient.h>

#define MOISTURE_PIN1 35
#define MOISTURE_PIN2 14
#define MOISTURE_PIN3 15
#define WATERLEVEL_PIN 16
#define MIN_MOISTURE 4093.0
#define MAX_MOISTURE 1024.0
#define MAX_WATER 445.0
#define DATA_SAVE_PERIOD 720
#define NUM_READINGS 72


// Replace with your network credentials
const char* ssid = "SPSETUP-9FD9";
const char* password = "appear6305drain";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings, sensorData;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;
unsigned long recordDataCount = 0;
int j, readPtr, writePtr, queueLen = 0;
int tempBuffer[NUM_READINGS] = {0};
int moistureBuffer[NUM_READINGS] = {0};
double waterBuffer[NUM_READINGS] = {0};

double waterLevel = 0;
int temp, lastWatering, moistureLevel = 0;

// Function prototypes
void initWiFi();
void initSPIFFS();
void initWebSocket();
void updateSite();
String readMostRecentData(int numLines, bool firstRead);
int countLinesInFile(File file);
void notifyClients(String sensorReadings);
String getSensorReadings();
int getAvgMoisture(int m1, int m2, int m3);
int getLastWatering();
float getWaterLevel(int wSensor);
double getTemp();
void storeData(float moistureLevel, float waterLevel, int temp);
String httpGETRequest(String serverName);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

// Get Sensor Readings and return JSON object
String getSensorReadings(){

  double moistureLevel = getAvgMoisture(analogRead(MOISTURE_PIN1), analogRead(MOISTURE_PIN2), analogRead(MOISTURE_PIN3));
  double waterLevel = getWaterLevel(analogRead(WATERLEVEL_PIN));
  lastWatering = getLastWatering();

  // store data in txt file for later extraction

  readings["type"] = String("cardData");
  readings["temperature"] = temp;
  readings["waterLevel"] =  waterLevel;
  readings["moistureLevel"] = moistureLevel;
  readings["waterDate"] = lastWatering;
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

int getLastWatering() {
  return 1;
}

int getAvgMoisture(int m1, int m2, int m3) {
  int avgRawReading = (m1+m2+m3)/3;
  float avgPct = map(avgRawReading, MIN_MOISTURE, MAX_MOISTURE, 0, 100);
  return avgPct;
}

float getWaterLevel(int wSensor) {
  return wSensor*100.0/MAX_WATER;
}

double getTemp() {

  String url = "https://api.openweathermap.org/data/2.5/weather?lat=49.28&lon=-123.12&appid=d04c686a7720ad0ba06256eb24dda19f&units=metric";

  String jsonBuffer = httpGETRequest(url);
  JSONVar myObject = JSON.parse(jsonBuffer);

  // JSON.typeof(jsonVar) can be used to get the type of the var
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return 0.0;
  }
  Serial.print("Temperature: ");
  Serial.println(myObject["main"]["temp"]);
  double returnTemp = (double)myObject["main"]["temp"];

  return returnTemp;
  
}

String httpGETRequest(String serverName) {
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void updateSite() {

  if ((millis() - lastTime) > timerDelay) {

    recordDataCount ++;

    if(recordDataCount == DATA_SAVE_PERIOD){
      temp = getTemp();
      recordDataCount = 0;
      String recentData = readMostRecentData(NUM_READINGS, false);
      Serial.println(recentData);
      notifyClients(recentData);
    }

    else {
      String sensorReadings = getSensorReadings();
      Serial.println(sensorReadings);
      notifyClients(sensorReadings);
    }

  lastTime = millis();

  }
}

void storeData(float moistureLevel, float waterLevel, int temp) {

  tempBuffer[writePtr] = temp;
  moistureBuffer[writePtr] = moistureLevel;
  waterBuffer[writePtr] = waterLevel;
  writePtr++;

  if(writePtr == NUM_READINGS) {
    writePtr = 0;
  }

  if(queueLen < NUM_READINGS){
    queueLen++;
  }

  else {
    readPtr++;
  }
}

String readMostRecentData(int numLines, bool firstRead) {

  int tempReadPtr;
  int i=0;

  if(!firstRead) {
    storeData(moistureLevel, waterLevel, temp);
  }

  if(readPtr == NUM_READINGS){
    readPtr = 0;
  }

  tempReadPtr = readPtr;

  // Create data arrays
  JSONVar temperatureArray;
  JSONVar moistureArray;
  JSONVar waterLevelArray;
  JSONVar sensorData;

  // fill 
  do {
    temperatureArray[i] = tempBuffer[tempReadPtr];
    moistureArray[i] = moistureBuffer[tempReadPtr];
    waterLevelArray[i] = waterBuffer[tempReadPtr];
    Serial.print("TempReadPtr: ");
    Serial.println(tempReadPtr);
    Serial.print("ReadPtr: ");
    Serial.println(readPtr);

    i++;
    tempReadPtr++;

    if(tempReadPtr >= queueLen){
      tempReadPtr = 0;
    }
  } while(tempReadPtr != readPtr);
      
  Serial.println();


  sensorData["type"] = "chartData";
  sensorData["temperature"] = temperatureArray;
  sensorData["moisture"] = moistureArray;
  sensorData["waterLevel"] = waterLevelArray;

  String jsonString = JSON.stringify(sensorData);
  return jsonString;
}

int countLinesInFile(File file) {
  int count = 0;
  while (file.available()) {
    file.readStringUntil('\n');
    count++;
  }
  return count;
}

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");

// create csv file on ESP32 for storing sensor data
  File dataFile = SPIFFS.open("/sensorData.csv", "w+");
  if (!dataFile) {
    Serial.println("Error opening sensor Data file for writing");
    return;
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      temp = getTemp();
      String sensorReadings = getSensorReadings();
      Serial.print(sensorReadings);
      notifyClients(sensorReadings);
      String recentData = readMostRecentData(NUM_READINGS, true);
      Serial.println(recentData);
      notifyClients(recentData);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initSPIFFS();
  initWebSocket();

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();
  temp = getTemp();
  getSensorReadings();
  storeData(moistureLevel, waterLevel, temp);
}

void loop() {

  updateSite();
  ws.cleanupClients();
}