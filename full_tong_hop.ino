// PHẦN 1: Khởi tạo & WebServer (setup() + loop() khung chính)
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Access Point
const char* ap_ssid = "bqh.local";
const char* ap_password = "12345678";
WebServer server(80);

// Preferences
Preferences prefs;

String gifSpeed = "5";
String gifDelay = "2";
bool galaxyOnly = false;
bool gifEnable = true;
String clockLocation = "Thu Duc, Ho Chi Minh";

// Biến điều khiển animation
unsigned long lastSwitch = 0;
int currentGifIndex = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println("MACH WiFi Setup...");
  display.display();

  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("WiFi AP started at http://bqh.local");

  // Load cấu hình từ Preferences
  prefs.begin("config", true);
  gifSpeed = prefs.getString("gifSpeed", "5");
  gifDelay = prefs.getString("gifDelay", "2");
  galaxyOnly = prefs.getBool("galaxyOnly", false);
  gifEnable = prefs.getBool("gifEnable", true);
  clockLocation = prefs.getString("clockLocation", "Thu Duc, Ho Chi Minh");
  prefs.end();

  // Khai báo các route web
  server.on("/", handleRoot);
  server.on("/gif", handleGIFPage);
  server.on("/clock", handleClockPage);
  server.on("/wifi", handleWiFiPage);
  server.on("/about", handleAboutPage);
  server.on("/reset", handleResetPage);
  server.on("/save", HTTP_POST, handleSaveConfig);

  server.begin();
  Serial.println("Web server running.");
}

void loop() {
  server.handleClient();

  if (galaxyOnly) {
    runGalaxyDisplay();
  } else if (gifEnable) {
    runGIF60();
  } else {
    runOledClock();
  }
}
