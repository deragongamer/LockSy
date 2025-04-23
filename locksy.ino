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
const char* ssid = "Locksy-Setup";  // Default AP name
const char* password = "12345678";   // Default AP password
const char* wifi_ssid = "";         // WiFi credentials (stored in config)
const char* wifi_password = "";      // WiFi credentials (stored in config)
IPAddress local_ip(192,168,4,1);    // AP mode IP
IPAddress gateway(192,168,4,1);     // AP mode gateway
IPAddress subnet(255,255,255,0);    // AP mode subnet

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

// Global variables
String lastAccessTime = "Never";

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
  
  // Try to connect to stored WiFi
  if(strlen(wifi_ssid) > 0) {
    WiFi.begin(wifi_ssid, wifi_password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
  }
  
  // If WiFi connection fails, start AP mode
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("\nStarting AP Mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(ssid, password);
    Serial.println("AP Started");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
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
  // Serve static files
  server.serveStatic("/", LittleFS, "/", "max-age=86400");
  
  // API endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/logs", HTTP_GET, handleLogs);
  server.on("/api/command/lock", HTTP_POST, handleLock);
  server.on("/api/command/unlock", HTTP_POST, handleUnlock);
  server.on("/api/command/reboot", HTTP_POST, handleReboot);
  
  // Handle 404
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
}

// Route handlers
void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  server.streamFile(file, "text/html");
  file.close();
}

void handleStatus() {
  StaticJsonDocument<200> doc;
  
  // Get door status
  bool doorOpen = digitalRead(DOOR_SENSOR_PIN) == LOW;
  doc["doorStatus"] = doorOpen ? "Unlocked" : "Locked";
  
  // Get WiFi signal
  int rssi = WiFi.RSSI();
  int signal = map(rssi, -100, -50, 0, 100);
  doc["wifiSignal"] = signal;
  
  // Get last access time
  doc["lastAccess"] = lastAccessTime;
  
  // Get uptime
  doc["uptime"] = millis() / 1000;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleConfig() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  const char* ssid = doc["ssid"];
  const char* password = doc["password"];
  
  if (!ssid || !password) {
    server.send(400, "text/plain", "Missing SSID or password");
    return;
  }
  
  // Save WiFi credentials
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    server.send(500, "text/plain", "Failed to save config");
    return;
  }
  
  configFile.print(body);
  configFile.close();
  
  // Send response
  server.send(200, "application/json", "{\"success\":true}");
  
  // Restart ESP
  delay(1000);
  ESP.restart();
}

void handleLogs() {
  File logFile = LittleFS.open("/logs.json", "r");
  if (!logFile) {
    server.send(200, "application/json", "[]");
    return;
  }
  
  server.streamFile(logFile, "application/json");
  logFile.close();
}

void handleLock() {
  digitalWrite(RELAY_PIN, HIGH);
  logAccess("lock", "success");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleUnlock() {
  digitalWrite(RELAY_PIN, LOW);
  logAccess("unlock", "success");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleReboot() {
  server.send(200, "application/json", "{\"success\":true}");
  delay(1000);
  ESP.restart();
}

// Helper functions
void logAccess(const char* method, const char* status) {
  File logFile = LittleFS.open("/logs.json", "r");
  StaticJsonDocument<1024> doc;
  
  if (logFile) {
    DeserializationError error = deserializeJson(doc, logFile);
    logFile.close();
    
    if (error) {
      doc = JsonArray();
    }
  } else {
    doc = JsonArray();
  }
  
  JsonObject log = doc.createNestedObject();
  log["time"] = getTimeString();
  log["method"] = method;
  log["status"] = status;
  
  // Keep only last 100 logs
  while (doc.size() > 100) {
    doc.remove(0);
  }
  
  logFile = LittleFS.open("/logs.json", "w");
  if (logFile) {
    serializeJson(doc, logFile);
    logFile.close();
  }
}

String getTimeString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStr);
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
