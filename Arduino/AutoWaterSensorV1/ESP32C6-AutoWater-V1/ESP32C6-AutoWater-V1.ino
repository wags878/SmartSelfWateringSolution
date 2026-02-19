#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- CONFIGURATION ---
const char* HOME_SSID = "Your_Home_SSID";
const char* HOME_PASS = "Your_Home_Password";
const char* WORK_SSID = "Your_Work_SSID";
const char* WORK_PASS = "Your_Work_Password";

#define I2C_SDA 21
#define I2C_SCL 22
const int sensorPin = 35; 

// Calibration Constants (Adjust these after testing)
const int DRY_VALUE = 3400; 
const int WET_VALUE = 1400;

// Hardware Objects
WiFiMulti wifiMulti;
WebServer server(80);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Global Variables
int moisturePercent = 0;
String currentSSID = "None";

// --- WEB UI OPTIMIZATION ---
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='10'>"; // Auto-refresh 10s
  html += "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/water.css@2/out/water.css'>";
  html += "<title>Blade Tech | Plant Monitor</title></head><body>";
  html += "<h1>🌿 Plant Dashboard</h1>";
  html += "<blockquote><strong>Status:</strong> " + String(moisturePercent < 30 ? "⚠️ Needs Water" : "✅ Healthy") + "</blockquote>";
  html += "<h3>Current Moisture: " + String(moisturePercent) + "%</h3>";
  html += "<progress value='" + String(moisturePercent) + "' max='100' style='width:100%'></progress>";
  html += "<hr><p><small>Connected to: " + currentSSID + "<br>IP: " + WiFi.localIP().toString() + "</small></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// --- ANALOG OPTIMIZATION (Averaging) ---
int getMoisture() {
  long sum = 0;
  for(int i = 0; i < 16; i++) { // Sample 16 times to smooth jitter
    sum += analogRead(sensorPin);
    delay(5);
  }
  int avg = sum / 16;
  int pcnt = map(avg, DRY_VALUE, WET_VALUE, 0, 100);
  return constrain(pcnt, 0, 100);
}

void setup() {
  Serial.begin(115200);
  
  // 1. OLED Init
  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Init Failed");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,10);
  display.println("MRW AutoWater Sensor v3.0");
  display.display();

  // 2. ADC Optimization
  analogSetAttenuation(ADC_11db); // Full 0-3.3V range

// 3. WiFi Connectivity (Corrected with Quotes)
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP("441-Wi-Fi", "!Llamas15@");
  wifiMulti.addAP("Scabbard", "BTI@10820");
  wifiMulti.addAP("MRW-iPhone15Pro", "!Llamas15@");

  display.println("Connecting WiFi...");
  display.display();

  // Non-blocking WiFi check
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  currentSSID = WiFi.SSID();
  server.on("/", handleRoot);
  server.begin();
  
  Serial.println("\nReady at http://" + WiFi.localIP().toString());
}

void loop() {
  // Ensure we stay connected/handle re-connections
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  } else {
    wifiMulti.run();
  }

  // Update Data
  moisturePercent = getMoisture();

  // OLED Refresh
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Net: "); display.println(currentSSID);
  display.print("IP:  "); display.println(WiFi.localIP().toString());
  
  display.setTextSize(2);
  display.setCursor(0, 35);
  display.print("Soil: "); 
  display.print(moisturePercent);
  display.print("%");
  
  display.display();
  delay(1000); // 1-second loop cycle
}