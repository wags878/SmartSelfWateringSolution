#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <Wire.h>
#include <U8g2lib.h> // Switched for better UI
#include "time.h"

// --- CREDENTIALS ---
const char* www_username = "admin";
const char* www_password = "blade_tech_password";

// --- HARDWARE PINS ---
#define I2C_SDA 21
#define I2C_SCL 22
const int sensorPin = 35;
const int pumpPin = 18;

// --- CONFIGURABLE VARIABLES ---
int dryValue = 3400;
int wetValue = 1400;
int pumpRunSeconds = 5;
int dryLimitPC = 30;     // Hysteresis Start
int wetLimitPC = 50;     // Hysteresis Stop
bool nightModeActive = true;
int startQuietHour = 22; 
int endQuietHour = 7;    

// --- STATE VARIABLES ---
int moisturePercent = 0;
bool isPumpRunning = false;
unsigned long pumpStartTime = 0;
String systemStatus = "IDLE";
WiFiMulti wifiMulti;
WebServer server(80);

// Professional UI Driver
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- TIME SETTINGS ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000; 
const int   daylightOffset_sec = 3600;

bool isQuietTime() {
  if (!nightModeActive) return false;
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return false;
  int hour = timeinfo.tm_hour;
  return (startQuietHour > endQuietHour) ? (hour >= startQuietHour || hour < endQuietHour) : (hour >= startQuietHour && hour < endQuietHour);
}

// --- WEB SERVER HANDLERS (Your Existing Logic) ---
void handleRoot() {
  if (!server.authenticate(www_username, www_password)) return server.requestAuthentication();
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/water.css@2/out/water.css'><title>Blade Tech Dashboard</title></head><body>";
  html += "<h1>🌿 Plant Dashboard</h1><h3>Moisture: " + String(moisturePercent) + "% " + (isQuietTime() ? "🌙 (Night Mode)" : "") + "</h3>";
  html += "<h2>Pump: " + systemStatus + "</h2><form action='/toggle' method='POST'>";
  if (!isPumpRunning) {
    html += "Run Duration: <input type='number' name='dur' value='" + String(pumpRunSeconds) + "' min='1' max='30'> ";
    html += "<button type='submit' " + String(isQuietTime() ? "disabled" : "") + ">Start Pump</button>";
  } else { html += "<button type='submit' style='background-color:red'>EMERGENCY STOP</button>"; }
  html += "</form><h2>Calibration</h2><form action='/calibrate' method='POST'>";
  html += "Dry Analog: <input type='number' name='dry' value='" + String(dryValue) + "'>Wet Analog: <input type='number' name='wet' value='" + String(wetValue) + "'>";
  html += "<button type='submit'>Save Values</button></form></body></html>";
  server.send(200, "text/html", html);
}

void handleCalibrate() {
  if (server.hasArg("dry")) dryValue = server.arg("dry").toInt();
  if (server.hasArg("wet")) wetValue = server.arg("wet").toInt();
  server.sendHeader("Location", "/"); server.send(303);
}

void handleToggle() {
  if (isQuietTime() && !isPumpRunning) { server.send(200, "text/plain", "Night Mode Active."); return; }
  if (!isPumpRunning) {
    if (server.hasArg("dur")) pumpRunSeconds = server.arg("dur").toInt();
    startPump();
  } else { stopPump(); }
  server.sendHeader("Location", "/"); server.send(303);
}

// --- CORE LOGIC ENHANCEMENTS ---
void startPump() {
  isPumpRunning = true;
  pumpStartTime = millis();
  digitalWrite(pumpPin, HIGH); 
  systemStatus = "PUMPING";
}

void stopPump() {
  isPumpRunning = false;
  digitalWrite(pumpPin, LOW);
  systemStatus = "IDLE";
}

int getSmoothedMoisture() {
  long sum = 0;
  for(int i=0; i<16; i++) { sum += analogRead(sensorPin); delay(2); }
  int pcnt = map(sum/16, dryValue, wetValue, 0, 100);
  return constrain(pcnt, 0, 100);
}

void updateUI() {
  u8g2.clearBuffer();
  // Header Zone
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, WiFi.localIP().toString().c_str());
  if(isQuietTime()) u8g2.drawStr(95, 10, "Zzz");

  // Center Hero Number
  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.setCursor(0, 46);
  u8g2.print(moisturePercent);
  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.print("%");

  // Status Info
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(75, 32);
  u8g2.print(systemStatus);

  // Visual Bar Gauge
  u8g2.drawFrame(0, 54, 128, 10);
  u8g2.drawBox(2, 56, map(moisturePercent, 0, 100, 0, 124), 6);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(pumpPin, OUTPUT);
  stopPump();
  
  Wire.begin(I2C_SDA, I2C_SCL);
  u8g2.begin();

  wifiMulti.addAP("441-Wi-Fi", "!Llamas15@");
  wifiMulti.addAP("Scabbard", "BTI@10820");
  wifiMulti.addAP("MRW-iPhone15Pro", "!Llamas15@");
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 20, "Connecting WiFi...");
  u8g2.sendBuffer();

  while (wifiMulti.run() != WL_CONNECTED) { delay(500); }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/", handleRoot);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.begin();
}

void loop() {
  server.handleClient();
  moisturePercent = getSmoothedMoisture();

  // Automatic Pump Logic with Hysteresis
  if (!isQuietTime()) {
    if (!isPumpRunning && moisturePercent < dryLimitPC) {
      startPump();
    }
    if (isPumpRunning) {
      // Shut off if wet enough OR time limit reached
      if (moisturePercent >= wetLimitPC || (millis() - pumpStartTime > (pumpRunSeconds * 1000))) {
        stopPump();
      }
    }
  }

  updateUI();
  delay(100); 
}