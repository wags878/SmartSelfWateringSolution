#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"

// --- CREDENTIALS ---
const char* www_username = "admin";
const char* www_password = "blade_tech_password"; // CHANGE THIS

// --- HARDWARE PINS ---
#define I2C_SDA 21
#define I2C_SCL 22
const int sensorPin = 35;
const int pumpPin = 18;

// --- CONFIGURABLE VARIABLES ---
int dryValue = 3400;
int wetValue = 1400;
int pumpRunSeconds = 5;
bool nightModeActive = true;
int startQuietHour = 22; // 10 PM
int endQuietHour = 7;    // 7 AM

// --- STATE VARIABLES ---
int moisturePercent = 0;
bool isPumpRunning = false;
unsigned long pumpStartTime = 0;
WiFiMulti wifiMulti;
WebServer server(80);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// --- TIME SETTINGS ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000; // Adjust for your timezone (e.g., -18000 for EST)
const int   daylightOffset_sec = 3600;

bool isQuietTime() {
  if (!nightModeActive) return false;
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return false;
  int hour = timeinfo.tm_hour;
  if (startQuietHour > endQuietHour) {
    return (hour >= startQuietHour || hour < endQuietHour);
  } else {
    return (hour >= startQuietHour && hour < endQuietHour);
  }
}

void handleRoot() {
  // --- AUTHENTICATION CHECK ---
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/water.css@2/out/water.css'>";
  html += "<title>Blade Tech Dashboard</title></head><body>";
  
  html += "<h1>🌿 Plant Dashboard</h1>";
  html += "<h3>Moisture: " + String(moisturePercent) + "% " + (isQuietTime() ? "🌙 (Night Mode)" : "") + "</h3>";

  // Pump Control
  html += "<h2>Pump Control</h2>";
  html += "<form action='/toggle' method='POST'>";
  if (!isPumpRunning) {
    html += "Run Duration: <input type='number' name='dur' value='" + String(pumpRunSeconds) + "' min='1' max='30'> ";
    html += "<button type='submit' " + String(isQuietTime() ? "disabled" : "") + ">Start Pump</button>";
  } else {
    html += "<button type='submit' style='background-color:red'>EMERGENCY STOP</button>";
  }
  html += "</form>";

  // Calibration Section
  html += "<h2>Calibration Settings</h2>";
  html += "<form action='/calibrate' method='POST'>";
  html += "Dry Value: <input type='number' name='dry' value='" + String(dryValue) + "'>";
  html += "Wet Value: <input type='number' name='wet' value='" + String(wetValue) + "'>";
  html += "<button type='submit'>Save Values</button>";
  html += "</form>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCalibrate() {
  if (server.hasArg("dry")) dryValue = server.arg("dry").toInt();
  if (server.hasArg("wet")) wetValue = server.arg("wet").toInt();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleToggle() {
  if (isQuietTime() && !isPumpRunning) {
    server.send(200, "text/plain", "Night Mode is Active. Pump disabled.");
    return;
  }
  if (!isPumpRunning) {
    if (server.hasArg("dur")) pumpRunSeconds = server.arg("dur").toInt();
    isPumpRunning = true;
    pumpStartTime = millis();
    digitalWrite(pumpPin, HIGH);
  } else {
    isPumpRunning = false;
    digitalWrite(pumpPin, LOW);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  wifiMulti.addAP("SSID", "PASS"); // Add your real SSIDs
  while (wifiMulti.run() != WL_CONNECTED) { delay(500); }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", handleRoot);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.begin();
}

void loop() {
  server.handleClient();
  
  // Update Moisture using variables
  int raw = analogRead(sensorPin);
  moisturePercent = map(raw, dryValue, wetValue, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  if (isPumpRunning && (millis() - pumpStartTime > (pumpRunSeconds * 1000))) {
    digitalWrite(pumpPin, LOW);
    isPumpRunning = false;
  }

  // OLED Updates...
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("IP: "); display.println(WiFi.localIP());
  display.print("Moist: "); display.print(moisturePercent); display.println("%");
  if(isQuietTime()) display.println("NIGHT MODE");
  display.display();
  delay(500);
}