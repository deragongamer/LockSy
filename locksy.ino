/*
 * Locksy Bot - ESP Edition
 * Smart Lock System with Web Interface
 * 
 * Features:
 * - 4x4 Keypad support
 * - RFID RC522 integration
 * - OLED/LCD Display
 * - Web panel with Light/Dark mode
 * - WiFi connectivity
 * - OTA updates
 * - Logging system
 * - Configurable pins through web panel
 * - Hall sensor/Microswitch support
 * - Active High/Low relay and buzzer
 * - Status LEDs
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <MFRC522.h>
#include <Wire.h>
#include <SPI.h>
#include <Keypad.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>

// Default PIN Configuration (can be changed through web panel)
#define RFID_SS_PIN D4
#define RFID_RST_PIN D3
#define RELAY_PIN D8
#define BUZZER_PIN D0
#define DOOR_SENSOR_PIN D1
#define STATUS_LED_PIN D2

// WiFi credentials (will be configurable)
const char* ssid = "YourSSID";
const char* password = "YourPassword";

// Web server
ESP8266WebServer server(80);

// RFID instance
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {D5, D6, D7, D8}; // Connect to the row pinouts of the keypad
byte colPins[COLS] = {D0, D1, D2, D3}; // Connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Initialize filesystem
  if(!LittleFS.begin()) {
    Serial.println("Error mounting filesystem");
    return;
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  // Setup web server routes
  setupWebServer();
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  checkKeypad();
  checkRFID();
  checkDoorSensor();
}

// Web server setup
void setupWebServer() {
  // Routes will be defined here
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/logs", handleLogs);
}

// Route handlers
void handleRoot() {
  // Serve web interface
}

void handleStatus() {
  // Return device status as JSON
}

void handleConfig() {
  // Handle configuration updates
}

void handleLogs() {
  // Return access logs
}

// Input checks
void checkKeypad() {
  char key = keypad.getKey();
  if (key) {
    // Handle keypad input
  }
}

void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;
    
  // Handle RFID card
}

void checkDoorSensor() {
  // Monitor door state
}
