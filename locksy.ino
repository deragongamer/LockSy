// locksy.ino
// Complete ESP8266 firmware for LockSy Control Panel

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Ticker.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Pin definitions (can be overridden via settings)
#define PIN_SDA D4
#define PIN_RST D3
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4
byte rowPins[KEYPAD_ROWS] = {D5, D6, D7, D8};
byte colPins[KEYPAD_COLS] = {D1, D2, D3, D4};
char keymap[KEYPAD_ROWS][KEYPAD_COLS] = {{'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'}};

// Global objects
MFRC522 rfid(PIN_SDA, PIN_RST);
Keypad keypad = Keypad(makeKeymap(keymap), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
AsyncWebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
Ticker rebootTicker;

// Settings file
const char* SETTINGS_FILE = "/settings.json";

// Data structures
struct Settings {
  String ssid;
  String pass;
  String user;
  String pwd;
  String readerType;
  uint8_t w0, w1;
  uint16_t autoLockSec;
  String autoRebootTime;
  String deviceModel;
} settings;

struct User { String uid; String name; };
std::vector<User> users;

struct LogEntry { String time; String uid; String result; };
std::vector<LogEntry> logs;

// NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// Forward declarations
void loadSettings();
void saveSettings();
void loadUsers();
void saveUsers();
void loadLogs();
void saveLogs();
void scheduleReboot();
void handleLock(bool lockState);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  MFRC522::PCD_Init();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("LockSy-Setup");

  if(!LittleFS.begin()) {
    Serial.println("LittleFS mount failed"); return;
  }
  loadSettings();
  loadUsers();
  loadLogs();

  timeClient.begin();

  // Static file serving
  server.serveStatic("/", LittleFS, "/data/").setDefaultFile("login.html");

  // API endpoints
  server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", (String)"{\"success\":true}"); // frontend handles creds
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    DynamicJsonDocument doc(512);
    doc["serial"] = WiFi.macAddress();
    doc["model"] = settings.deviceModel;
    doc["firmware"] = "1.0.0";
    doc["time"] = timeClient.getFormattedTime();
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *req){
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.createNestedArray("networks");
    for(int i=0;i<n;i++) arr.add(WiFi.SSID(i));
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("plain", true)){
      String body = req->getParam("plain", true)->value();
      DynamicJsonDocument doc(512);
      deserializeJson(doc, body);
      settings.ssid = doc["ssid"].as<String>();
      settings.pass = doc["pass"].as<String>();
      saveSettings();
      WiFi.begin(settings.ssid, settings.pass);
      req->send(200, "application/json", "{\"status\":\"ok\"}");
    }
  });

  server.on("/api/hardware/config", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("plain", true)){
      String body = req->getParam("plain", true)->value();
      DynamicJsonDocument doc(512);
      deserializeJson(doc, body);
      settings.readerType = doc["readerType"].as<String>();
      settings.w0 = doc["w0"];
      settings.w1 = doc["w1"];
      settings.deviceModel = doc["model"].as<String>();
      saveSettings();
      req->send(200, "application/json", "{\"status\":\"ok\"}");
    }
  });

  server.on("/api/users", HTTP_GET, [](AsyncWebServerRequest *req){
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.createNestedArray("users");
    for(auto &u: users){
      JsonObject obj = arr.createNestedObject(); obj["uid"] = u.uid; obj["name"] = u.name;
    }
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/users", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("plain", true)){
      String body = req->getParam("plain", true)->value();
      DynamicJsonDocument doc(256); deserializeJson(doc, body);
      users.push_back({doc["uid"].as<String>(), doc["name"].as<String>()});
      saveUsers(); req->send(200, "application/json", "{\"status\":\"ok\"}");
    }
  });

  server.on("/api/users", HTTP_DELETE, [](AsyncWebServerRequest *req){
    if(req->hasParam("uid")){
      String uid = req->getParam("uid")->value();
      users.erase(std::remove_if(users.begin(), users.end(), [&](User &u){ return u.uid == uid;}), users.end());
      saveUsers(); req->send(200, "application/json", "{\"status\":\"ok\"}");
    }
  });

  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *req){
    String csv = "time,uid,result\n";
    for(auto &e: logs) csv += e.time + "," + e.uid + "," + e.result + "\n";
    req->send(200, "text/csv", csv);
  });
  server.on("/api/logs", HTTP_DELETE, [](AsyncWebServerRequest *req){ logs.clear(); saveLogs(); req->send(200, "application/json", "{\"status\":\"cleared\"}"); });

  server.on("/api/control/lock", HTTP_POST, [](AsyncWebServerRequest *req){ handleLock(true); req->send(200); });
  server.on("/api/control/unlock", HTTP_POST, [](AsyncWebServerRequest *req){ handleLock(false); req->send(200); });

  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req){
    if(req->hasParam("plain", true)){
      String body = req->getParam("plain", true)->value();
      DynamicJsonDocument doc(256); deserializeJson(doc, body);
      settings.autoLockSec = doc["autoLock"];
      settings.autoRebootTime = doc["rebootTime"].as<String>();
      saveSettings(); scheduleReboot();
      req->send(200, "application/json", "{\"status\":\"saved\"}");
    }
  });

  httpUpdater.setup(&server);
  server.begin();
  scheduleReboot();
}

void loop() {
  timeClient.update();
}

// Helper implementations
void loadSettings() {
  File f = LittleFS.open(SETTINGS_FILE, "r");
  if(!f){ // defaults
    settings = {"","","admin","1234","rc522", PIN_SDA, PIN_RST, 30, "00:00", "NodeMCU v1"};
    saveSettings(); return;
  }
  DynamicJsonDocument doc(512); deserializeJson(doc, f); f.close();
  settings.ssid = doc["ssid"].as<String>();
  settings.pass = doc["pass"].as<String>();
  settings.user = doc["user"].as<String>();
  settings.pwd = doc["pwd"].as<String>();
  settings.readerType = doc["readerType"].as<String>();
  settings.w0 = doc["w0"];
  settings.w1 = doc["w1"];
  settings.autoLockSec = doc["autoLock"];
  settings.autoRebootTime = doc["rebootTime"].as<String>();
  settings.deviceModel = doc["model"].as<String>();
}

void saveSettings() {
  File f = LittleFS.open(SETTINGS_FILE, "w");
  DynamicJsonDocument doc(512);
  doc["ssid"] = settings.ssid;
  doc["pass"] = settings.pass;
  doc["user"] = settings.user;
  doc["pwd"] = settings.pwd;
  doc["readerType"] = settings.readerType;
  doc["w0"] = settings.w0;
  doc["w1"] = settings.w1;
  doc["autoLock"] = settings.autoLockSec;
  doc["rebootTime"] = settings.autoRebootTime;
  doc["model"] = settings.deviceModel;
  serializeJson(doc, f);
  f.close();
}

void loadUsers() {
  if(!LittleFS.exists("/users.json")) return;
  File f = LittleFS.open("/users.json","r");
  DynamicJsonDocument doc(1024); deserializeJson(doc,f); f.close();
  for(auto u: doc["users"].as<JsonArray>()) users.push_back({u["uid"].as<String>(), u["name"].as<String>()});
}

void saveUsers() {
  File f = LittleFS.open("/users.json","w");
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.createNestedArray("users");
  for(auto &u: users){ auto o = arr.createNestedObject(); o["uid"] = u.uid; o["name"] = u.name; }
  serializeJson(doc,f); f.close();
}

void loadLogs() {
  if(!LittleFS.exists("/logs.json")) return;
  File f = LittleFS.open("/logs.json","r");
  DynamicJsonDocument doc(2048); deserializeJson(doc,f); f.close();
  for(auto e: doc["logs"].as<JsonArray>()) logs.push_back({e["time"].as<String>(), e["uid"].as<String>(), e["result"].as<String>()});
}

void saveLogs() {
  File f = LittleFS.open("/logs.json","w");
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.createNestedArray("logs");
  for(auto &e: logs){ auto o = arr.createNestedObject(); o["time"]=e.time; o["uid"]=e.uid; o["result"]=e.result; }
  serializeJson(doc,f); f.close();
}

void scheduleReboot() {
  // parse HH:MM
  int hh = settings.autoRebootTime.substring(0,2).toInt();
  int mm = settings.autoRebootTime.substring(3,5).toInt();
  timeClient.update();
  int nowSec = timeClient.getHours()*3600 + timeClient.getMinutes()*60 + timeClient.getSeconds();
  int targetSec = hh*3600 + mm*60;
  int diff = (targetSec - nowSec + 86400) % 86400;
  rebootTicker.detach();
  rebootTicker.once(diff, [](){ ESP.restart(); });
}

void handleLock(bool lockState) {
  digitalWrite(D0, lockState); // example relay pin
  String result = lockState?"LOCKED":"UNLOCKED";
  timeClient.update();
  logs.push_back({timeClient.getFormattedTime(), "-", result});
  saveLogs();
}
