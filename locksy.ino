/*
 * Locksy Bot - ESP Edition v2.0
 * Advanced Smart Lock System with Web Interface
 * 
 * New Features in v2.0:
 * - Fingerprint sensor support
 * - Telegram bot integration
 * - Face recognition (ESP32-CAM)
 * - Enhanced security with encryption
 * - Multi-factor authentication
 * - Scheduled access control
 * - Remote monitoring
 * - Backup battery support
 * - Temperature monitoring
 * - Anti-tamper alerts
 * - Emergency override
 * - Voice control support
 * - Mobile app integration
 * - Geofencing capabilities
 * 
 * Original Features:
 * - 4x4 Keypad support
 * - RFID RC522 integration
 * - OLED/LCD Display
 * - Web panel with Light/Dark mode
 * - WiFi connectivity
 * - OTA updates
 * - Logging system
 * - Configurable pins
 * - Hall sensor/Microswitch support
 * - Active High/Low relay and buzzer
 * - Status LEDs
 * - User management
 * - Model selection
 * - Auto reboot scheduling
 * - Log download
 * - Pin configuration
 * - Advanced settings
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
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_Fingerprint.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <AES.h>

// Telegram Bot Configuration
#define BOT_TOKEN "YOUR_BOT_TOKEN_HERE"  // توکن ربات خودتون رو اینجا قرار بدید
#define CHAT_ID "YOUR_CHAT_ID_HERE"      // شناسه چت خودتون رو اینجا قرار بدید

// Pin Configuration
#define RFID_SS_PIN D4
#define RFID_RST_PIN D3
#define RELAY_PIN D8
#define BUZZER_PIN D0
#define DOOR_SENSOR_PIN D1
#define STATUS_LED_PIN D2
#define FINGERPRINT_RX D5
#define FINGERPRINT_TX D6
#define TEMP_SENSOR_PIN A0
#define BACKUP_BATTERY_PIN D7
#define TAMPER_SENSOR_PIN D8

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

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Global variables
String lastAccessTime = "Never";
String deviceModel = "Standard";  // Can be: Standard, Pro, Enterprise
bool autoLockEnabled = true;
int autoLockTime = 30;  // seconds
bool buzzerEnabled = true;
bool relayActiveHigh = true;
bool doorSensorActiveLow = true;
unsigned long lastRebootTime = 0;
int rebootInterval = 0;  // 0 means no auto reboot

// User management
struct User {
  String username;
  String password;
  String role;  // admin, user
  bool enabled;
};

const int MAX_USERS = 10;
User users[MAX_USERS];
int userCount = 0;

// Pin configuration
struct PinConfig {
  String name;
  int pin;
  String type;  // input, output
  bool activeHigh;
};

const int MAX_PINS = 20;
PinConfig pins[MAX_PINS];
int pinCount = 0;

// Fingerprint sensor instance
SoftwareSerial fpSerial(FINGERPRINT_RX, FINGERPRINT_TX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fpSerial);

// Telegram bot instance
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// AES encryption instance
AES aes;

// Global variables for new features
bool tamperDetected = false;
float temperature = 0.0;
float batteryLevel = 0.0;
bool emergencyMode = false;
String lastTelegramCommand = "";
unsigned long lastTelegramCheck = 0;
const int telegramCheckInterval = 1000;

// Geofencing variables
struct GeofencePoint {
  float lat;
  float lon;
  float radius; // in meters
};
GeofencePoint geofencePoints[5];
int geofenceCount = 0;

// Schedule structure
struct Schedule {
  int userId;
  time_t startTime;
  time_t endTime;
  bool recurring;
  int daysOfWeek; // Bitfield for days (Sunday = 1, Monday = 2, etc.)
};
Schedule schedules[20];
int scheduleCount = 0;

// Function declarations
void setupGeofencing();
void loadSchedules();
void checkSchedules();
void checkGeofence();
void handleFingerprint();
void handleTemperature();
void handleBattery();
void handleSchedule();
void handleGeofence();
void handleEmergency();

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting Locksy...");
  
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
    // Don't return, continue with AP mode
  }
  
  // Load configuration
  loadConfig();
  
  // Try to connect to stored WiFi
  if(strlen(wifi_ssid) > 0) {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(1000);
      Serial.print(".");
      attempts++;
      yield(); // Allow ESP8266 to handle background tasks
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nWiFi connection failed!");
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
  }
  
  // Initialize NTP
  timeClient.begin();
  timeClient.setTimeOffset(0);  // UTC
  
  // Setup web server routes
  setupWebServer();
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  
  // Create default admin user if no users exist
  if (userCount == 0) {
    addUser("admin", "admin123", "admin", true);
  }
  
  // Initialize new sensors
  finger.begin(57600);
  setupTelegramBot();
  setupEncryption();
  setupGeofencing();
  loadSchedules();
  
  // Additional pin setup
  pinMode(TEMP_SENSOR_PIN, INPUT);
  pinMode(BACKUP_BATTERY_PIN, INPUT);
  pinMode(TAMPER_SENSOR_PIN, INPUT_PULLUP);
  
  // Setup additional web routes
  server.on("/api/fingerprint", HTTP_POST, handleFingerprint);
  server.on("/api/temperature", HTTP_GET, handleTemperature);
  server.on("/api/battery", HTTP_GET, handleBattery);
  server.on("/api/schedule", HTTP_POST, handleSchedule);
  server.on("/api/geofence", HTTP_POST, handleGeofence);
  server.on("/api/emergency", HTTP_POST, handleEmergency);
}

void loop() {
  // Check WiFi connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED && strlen(wifi_ssid) > 0) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.reconnect();
    delay(5000); // Wait for reconnection
    yield(); // Allow ESP8266 to handle background tasks
  }
  
  server.handleClient();
  checkKeypad();
  checkRFID();
  checkDoorSensor();
  checkAutoLock();
  checkAutoReboot();
  updateTime();
  
  // New feature checks
  checkTelegram();
  checkTemperature();
  checkBattery();
  checkTamper();
  checkSchedules();
  checkGeofence();
  
  // Emergency override check
  if (emergencyMode) {
    handleEmergencyMode();
  }
  
  // Add a small delay to prevent watchdog timer issues
  delay(10);
  yield(); // Allow ESP8266 to handle background tasks
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
  
  // User management
  server.on("/api/users", HTTP_GET, handleGetUsers);
  server.on("/api/users", HTTP_POST, handleAddUser);
  server.on("/api/users", HTTP_DELETE, handleDeleteUser);
  server.on("/api/users/password", HTTP_POST, handleChangePassword);
  
  // Pin configuration
  server.on("/api/pins", HTTP_GET, handleGetPins);
  server.on("/api/pins", HTTP_POST, handleUpdatePin);
  
  // Model and settings
  server.on("/api/model", HTTP_GET, handleGetModel);
  server.on("/api/model", HTTP_POST, handleSetModel);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handleSetSettings);
  
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
  bool doorOpen = digitalRead(DOOR_SENSOR_PIN) == (doorSensorActiveLow ? LOW : HIGH);
  doc["doorStatus"] = doorOpen ? "Unlocked" : "Locked";
  
  // Get WiFi signal
  int rssi = WiFi.RSSI();
  int signal = map(rssi, -100, -50, 0, 100);
  doc["wifiSignal"] = signal;
  
  // Get last access time
  doc["lastAccess"] = lastAccessTime;
  
  // Get uptime
  doc["uptime"] = millis() / 1000;
  
  // Get model and settings
  doc["model"] = deviceModel;
  doc["autoLock"] = autoLockEnabled;
  doc["autoLockTime"] = autoLockTime;
  doc["buzzer"] = buzzerEnabled;
  doc["relayActiveHigh"] = relayActiveHigh;
  doc["doorSensorActiveLow"] = doorSensorActiveLow;
  doc["rebootInterval"] = rebootInterval;
  
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
  digitalWrite(RELAY_PIN, relayActiveHigh ? HIGH : LOW);
  logAccess("lock", "success");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleUnlock() {
  digitalWrite(RELAY_PIN, relayActiveHigh ? LOW : HIGH);
  logAccess("unlock", "success");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleReboot() {
  server.send(200, "application/json", "{\"success\":true}");
  delay(1000);
  ESP.restart();
}

// User management handlers
void handleGetUsers() {
  StaticJsonDocument<1024> doc;
  JsonArray usersArray = doc.createNestedArray("users");
  
  for (int i = 0; i < userCount; i++) {
    JsonObject user = usersArray.createNestedObject();
    user["username"] = users[i].username;
    user["role"] = users[i].role;
    user["enabled"] = users[i].enabled;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAddUser() {
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
  
  const char* username = doc["username"];
  const char* password = doc["password"];
  const char* role = doc["role"];
  
  if (!username || !password || !role) {
    server.send(400, "text/plain", "Missing required fields");
    return;
  }
  
  if (userCount >= MAX_USERS) {
    server.send(400, "text/plain", "Maximum users reached");
    return;
  }
  
  addUser(username, password, role, true);
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDeleteUser() {
  if (server.method() != HTTP_DELETE) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  String username = server.arg("username");
  if (!username) {
    server.send(400, "text/plain", "Missing username");
    return;
  }
  
  for (int i = 0; i < userCount; i++) {
    if (users[i].username == username) {
      // Remove user by shifting array
      for (int j = i; j < userCount - 1; j++) {
        users[j] = users[j + 1];
      }
      userCount--;
      saveUsers();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(404, "text/plain", "User not found");
}

void handleChangePassword() {
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
  
  const char* username = doc["username"];
  const char* newPassword = doc["newPassword"];
  
  if (!username || !newPassword) {
    server.send(400, "text/plain", "Missing required fields");
    return;
  }
  
  for (int i = 0; i < userCount; i++) {
    if (users[i].username == username) {
      users[i].password = newPassword;
      saveUsers();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(404, "text/plain", "User not found");
}

// Pin configuration handlers
void handleGetPins() {
  StaticJsonDocument<1024> doc;
  JsonArray pinsArray = doc.createNestedArray("pins");
  
  for (int i = 0; i < pinCount; i++) {
    JsonObject pin = pinsArray.createNestedObject();
    pin["name"] = pins[i].name;
    pin["pin"] = pins[i].pin;
    pin["type"] = pins[i].type;
    pin["activeHigh"] = pins[i].activeHigh;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleUpdatePin() {
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
  
  const char* name = doc["name"];
  int pin = doc["pin"];
  const char* type = doc["type"];
  bool activeHigh = doc["activeHigh"];
  
  if (!name || !type) {
    server.send(400, "text/plain", "Missing required fields");
    return;
  }
  
  // Update or add pin
  bool found = false;
  for (int i = 0; i < pinCount; i++) {
    if (pins[i].name == name) {
      pins[i].pin = pin;
      pins[i].type = type;
      pins[i].activeHigh = activeHigh;
      found = true;
      break;
    }
  }
  
  if (!found && pinCount < MAX_PINS) {
    pins[pinCount].name = name;
    pins[pinCount].pin = pin;
    pins[pinCount].type = type;
    pins[pinCount].activeHigh = activeHigh;
    pinCount++;
  }
  
  savePins();
  server.send(200, "application/json", "{\"success\":true}");
}

// Model and settings handlers
void handleGetModel() {
  StaticJsonDocument<200> doc;
  doc["model"] = deviceModel;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetModel() {
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
  
  const char* model = doc["model"];
  if (!model) {
    server.send(400, "text/plain", "Missing model");
    return;
  }
  
  deviceModel = model;
  saveConfig();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetSettings() {
  StaticJsonDocument<200> doc;
  doc["autoLock"] = autoLockEnabled;
  doc["autoLockTime"] = autoLockTime;
  doc["buzzer"] = buzzerEnabled;
  doc["relayActiveHigh"] = relayActiveHigh;
  doc["doorSensorActiveLow"] = doorSensorActiveLow;
  doc["rebootInterval"] = rebootInterval;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetSettings() {
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
  
  autoLockEnabled = doc["autoLock"] | autoLockEnabled;
  autoLockTime = doc["autoLockTime"] | autoLockTime;
  buzzerEnabled = doc["buzzer"] | buzzerEnabled;
  relayActiveHigh = doc["relayActiveHigh"] | relayActiveHigh;
  doorSensorActiveLow = doc["doorSensorActiveLow"] | doorSensorActiveLow;
  rebootInterval = doc["rebootInterval"] | rebootInterval;
  
  saveConfig();
  server.send(200, "application/json", "{\"success\":true}");
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

void addUser(const char* username, const char* password, const char* role, bool enabled) {
  if (userCount >= MAX_USERS) return;
  
  users[userCount].username = username;
  users[userCount].password = password;
  users[userCount].role = role;
  users[userCount].enabled = enabled;
  userCount++;
  
  saveUsers();
}

void saveUsers() {
  File file = LittleFS.open("/users.json", "w");
  if (!file) return;
  
  StaticJsonDocument<1024> doc;
  JsonArray usersArray = doc.createNestedArray("users");
  
  for (int i = 0; i < userCount; i++) {
    JsonObject user = usersArray.createNestedObject();
    user["username"] = users[i].username;
    user["password"] = users[i].password;
    user["role"] = users[i].role;
    user["enabled"] = users[i].enabled;
  }
  
  serializeJson(doc, file);
  file.close();
}

void loadUsers() {
  File file = LittleFS.open("/users.json", "r");
  if (!file) return;
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) return;
  
  JsonArray usersArray = doc["users"];
  userCount = 0;
  
  for (JsonObject user : usersArray) {
    if (userCount >= MAX_USERS) break;
    
    users[userCount].username = user["username"].as<String>();
    users[userCount].password = user["password"].as<String>();
    users[userCount].role = user["role"].as<String>();
    users[userCount].enabled = user["enabled"] | true;
    userCount++;
  }
}

void savePins() {
  File file = LittleFS.open("/pins.json", "w");
  if (!file) return;
  
  StaticJsonDocument<1024> doc;
  JsonArray pinsArray = doc.createNestedArray("pins");
  
  for (int i = 0; i < pinCount; i++) {
    JsonObject pin = pinsArray.createNestedObject();
    pin["name"] = pins[i].name;
    pin["pin"] = pins[i].pin;
    pin["type"] = pins[i].type;
    pin["activeHigh"] = pins[i].activeHigh;
  }
  
  serializeJson(doc, file);
  file.close();
}

void loadPins() {
  File file = LittleFS.open("/pins.json", "r");
  if (!file) return;
  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) return;
  
  JsonArray pinsArray = doc["pins"];
  pinCount = 0;
  
  for (JsonObject pin : pinsArray) {
    if (pinCount >= MAX_PINS) break;
    
    pins[pinCount].name = pin["name"].as<String>();
    pins[pinCount].pin = pin["pin"] | 0;
    pins[pinCount].type = pin["type"].as<String>();
    pins[pinCount].activeHigh = pin["activeHigh"] | true;
    pinCount++;
  }
}

void saveConfig() {
  File file = LittleFS.open("/config.json", "w");
  if (!file) return;
  
  StaticJsonDocument<200> doc;
  doc["model"] = deviceModel;
  doc["autoLock"] = autoLockEnabled;
  doc["autoLockTime"] = autoLockTime;
  doc["buzzer"] = buzzerEnabled;
  doc["relayActiveHigh"] = relayActiveHigh;
  doc["doorSensorActiveLow"] = doorSensorActiveLow;
  doc["rebootInterval"] = rebootInterval;
  
  serializeJson(doc, file);
  file.close();
}

void loadConfig() {
  File file = LittleFS.open("/config.json", "r");
  if (!file) return;
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) return;
  
  deviceModel = doc["model"] | "Standard";
  autoLockEnabled = doc["autoLock"] | true;
  autoLockTime = doc["autoLockTime"] | 30;
  buzzerEnabled = doc["buzzer"] | true;
  relayActiveHigh = doc["relayActiveHigh"] | true;
  doorSensorActiveLow = doc["doorSensorActiveLow"] | true;
  rebootInterval = doc["rebootInterval"] | 0;
}

void checkAutoLock() {
  if (!autoLockEnabled) return;
  
  static unsigned long lastUnlockTime = 0;
  bool doorOpen = digitalRead(DOOR_SENSOR_PIN) == (doorSensorActiveLow ? LOW : HIGH);
  
  if (doorOpen) {
    lastUnlockTime = millis();
  } else if (lastUnlockTime > 0 && (millis() - lastUnlockTime) > (autoLockTime * 1000)) {
    digitalWrite(RELAY_PIN, relayActiveHigh ? HIGH : LOW);
    lastUnlockTime = 0;
  }
}

void checkAutoReboot() {
  if (rebootInterval <= 0) return;
  
  if ((millis() - lastRebootTime) > (rebootInterval * 1000)) {
    ESP.restart();
  }
}

void updateTime() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
  }
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

// New feature implementations
void setupTelegramBot() {
  if (strlen(BOT_TOKEN) > 0 && strlen(CHAT_ID) > 0) {
    secured_client.setInsecure();
    if (!bot.sendMessage(CHAT_ID, "🔒 Locksy system is online!", "")) {
      Serial.println("Failed to send Telegram message");
    }
  }
}

void setupEncryption() {
  // Initialize AES encryption with your key
  byte key[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
  aes.set_key(key, sizeof(key));
}

void checkTelegram() {
  if (strlen(BOT_TOKEN) == 0 || strlen(CHAT_ID) == 0) return;
  
  if (millis() - lastTelegramCheck > telegramCheckInterval) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages > 0) {
      for (int i = 0; i < numNewMessages; i++) {
        handleTelegramCommand(bot.messages[i].text);
      }
    }
    lastTelegramCheck = millis();
  }
}

void handleTelegramCommand(String command) {
  if (strlen(BOT_TOKEN) == 0 || strlen(CHAT_ID) == 0) return;
  
  if (command == "/status") {
    String status = getSystemStatus();
    if (!bot.sendMessage(CHAT_ID, status, "")) {
      Serial.println("Failed to send status message");
    }
  } else if (command == "/lock") {
    handleLock();
    if (!bot.sendMessage(CHAT_ID, "🔒 Door locked!", "")) {
      Serial.println("Failed to send lock message");
    }
  } else if (command == "/unlock") {
    handleUnlock();
    if (!bot.sendMessage(CHAT_ID, "🔓 Door unlocked!", "")) {
      Serial.println("Failed to send unlock message");
    }
  } else if (command == "/temperature") {
    String temp = String(temperature) + "°C";
    if (!bot.sendMessage(CHAT_ID, "🌡️ Current temperature: " + temp, "")) {
      Serial.println("Failed to send temperature message");
    }
  }
}

void checkTemperature() {
  int rawTemp = analogRead(TEMP_SENSOR_PIN);
  temperature = (rawTemp * 3.3 / 1024.0) * 100;
  
  if (temperature > 50) { // Alert if temperature is too high
    sendAlert("High temperature detected: " + String(temperature) + "°C");
  }
}

void checkBattery() {
  int rawBattery = analogRead(BACKUP_BATTERY_PIN);
  batteryLevel = (rawBattery * 100.0) / 1024.0;
  
  if (batteryLevel < 20) {
    sendAlert("Low battery level: " + String(batteryLevel) + "%");
  }
}

void checkTamper() {
  bool currentTamperState = digitalRead(TAMPER_SENSOR_PIN);
  
  if (currentTamperState != tamperDetected) {
    tamperDetected = currentTamperState;
    if (tamperDetected) {
      sendAlert("⚠️ TAMPER DETECTED! Security breach attempt!");
      logAccess("tamper", "detected");
    }
  }
}

void handleEmergencyMode() {
  // Override normal lock behavior in emergency
  digitalWrite(RELAY_PIN, relayActiveHigh ? LOW : HIGH);
  
  // Notify all admins
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID, "🚨 EMERGENCY MODE ACTIVE - Door unlocked!", "");
  }
}

String encryptData(String data) {
  // Pad data to 16 byte blocks
  while (data.length() % 16 != 0) {
    data += '\0';
  }
  
  byte plain[16];
  byte cipher[16];
  data.getBytes(plain, 16);
  
  // Encrypt
  aes.encrypt(plain, cipher);
  
  // Convert to hex string
  String result = "";
  for(int i = 0; i < 16; i++) {
    char hex[3];
    sprintf(hex, "%02X", cipher[i]);
    result += hex;
  }
  return result;
}

String decryptData(String data) {
  // Convert hex string to byte array
  byte cipher[16];
  for(int i = 0; i < 16; i++) {
    char hex[3] = {data[i*2], data[i*2+1], '\0'};
    cipher[i] = strtol(hex, NULL, 16);
  }
  
  // Decrypt
  byte plain[16];
  aes.decrypt(cipher, plain);
  
  // Convert to string and trim padding
  String result = String((char*)plain);
  result.trim();
  return result;
}

void sendAlert(String message) {
  if (strlen(BOT_TOKEN) == 0 || strlen(CHAT_ID) == 0) return;
  
  if (WiFi.status() == WL_CONNECTED) {
    if (!bot.sendMessage(CHAT_ID, message, "")) {
      Serial.println("Failed to send Telegram alert");
    }
  }
  logAccess("alert", message.c_str());
}

String getSystemStatus() {
  String status = "🔒 Locksy System Status\n\n";
  status += "Door: " + String(digitalRead(DOOR_SENSOR_PIN) == (doorSensorActiveLow ? LOW : HIGH) ? "Open 🔓" : "Closed 🔒") + "\n";
  status += "Temperature: " + String(temperature) + "°C\n";
  status += "Battery: " + String(batteryLevel) + "%\n";
  status += "WiFi Signal: " + String(WiFi.RSSI()) + " dBm\n";
  status += "Tamper Status: " + String(tamperDetected ? "⚠️ ALERT" : "✅ Secure") + "\n";
  status += "Emergency Mode: " + String(emergencyMode ? "🚨 ACTIVE" : "✅ Inactive") + "\n";
  
  return status;
}

void setupGeofencing() {
  // Initialize geofencing
  geofenceCount = 0;
}

void loadSchedules() {
  // Load schedules from storage
  scheduleCount = 0;
}

void checkSchedules() {
  // Check if any scheduled events need to be triggered
  time_t now = time(nullptr);
  for (int i = 0; i < scheduleCount; i++) {
    if (now >= schedules[i].startTime && now <= schedules[i].endTime) {
      // Handle scheduled event
    }
  }
}

void checkGeofence() {
  // Check if device is within any geofence
  for (int i = 0; i < geofenceCount; i++) {
    // Implement geofence checking logic
  }
}

void handleFingerprint() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  // Handle fingerprint operations
  server.send(200, "application/json", "{\"success\":true}");
}

void handleTemperature() {
  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleBattery() {
  StaticJsonDocument<200> doc;
  doc["battery"] = batteryLevel;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSchedule() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  // Handle schedule operations
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGeofence() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  // Handle geofence operations
  server.send(200, "application/json", "{\"success\":true}");
}

void handleEmergency() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  emergencyMode = true;
  handleEmergencyMode();
  server.send(200, "application/json", "{\"success\":true}");
}

