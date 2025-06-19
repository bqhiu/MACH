// ================================================= PHẦN 1/2 =================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ap_ssid = "bqh.local";
const char* ap_password = "12345678";
WebServer server(80);
Preferences prefs;

String gifSpeed = "5";
String gifDelay = "2";
bool galaxyOnly = false;
bool gifEnable = true;
String clockLocation = "Thu Duc";
String weatherApiKey = "d2386720d2ad0a3823385ec99bd27598";
float currentTemp = 0.0;
String weatherIcon = "☀";
unsigned long lastWeatherUpdate = 0;

#define SERVICE_UUID        "DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_INDICATE_UUID  "DD3F0AD2-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_WRITE_UUID     "DD3F0AD3-6239-4E1F-81F1-91F6C9F01D86"

BLECharacteristic* pWriteCharacteristic;
bool bleConnected = false;
bool bleHasData = false;
uint8_t bleDirection = 0x00;
uint32_t bleDistance = 0;
int frameIndex = 0;
const int totalFrames = 772;

void showWelcome() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  String text = "hello";
  for (int i = 0; i < text.length(); i++) {
    display.setCursor(20, 20);
    display.print(text.substring(0, i + 1));
    display.display();
    delay(300);
    display.clearDisplay();
  }
  display.setCursor(20, 20);
  display.print("hello");
  display.display();
  delay(1000);
  for (int i = 0; i < 3; i++) {
    display.clearDisplay();
    display.setCursor(40, 45);
    display.setTextSize(2);
    display.print("BQH");
    display.display();
    delay(250);
    display.clearDisplay();
    delay(250);
  }
}

void drawLeftArrow() {
  display.fillRect(58, 5, 11, 25, SSD1306_WHITE);
  display.fillRect(45, 5, 13, 10, SSD1306_WHITE);
  display.fillTriangle(32, 10, 45, 0, 45, 20, SSD1306_WHITE);
}

void drawRightArrow() {
  display.fillRect(58, 5, 11, 25, SSD1306_WHITE);
  display.fillRect(69, 5, 13, 10, SSD1306_WHITE);
  display.fillTriangle(95, 10, 82, 0, 82, 20, SSD1306_WHITE);
}

void drawStraightArrow() {
  display.fillRect(58, 10, 11, 19, SSD1306_WHITE);
  display.fillTriangle(63, 2, 47, 14, 79, 14, SSD1306_WHITE);
}

void drawDistanceBar(uint32_t d) {
  const int barX = 10, barY = 50, barW = 108, barH = 10;
  int fill = map(d, 0, 1000, 0, barW - 2);
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  display.fillRect(barX + 1, barY + 1, fill, barH - 2, SSD1306_WHITE);
}

void drawHUD() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Distance: ");
  display.print(bleDistance);
  display.print("m");

  if (bleDistance <= 30) {
    static bool blink = false;
    if (blink) {
      if (bleDirection == 0x08) drawLeftArrow();
      else if (bleDirection == 0x0A) drawRightArrow();
      else drawStraightArrow();
    }
    blink = !blink;
  } else {
    if (bleDirection == 0x08) drawLeftArrow();
    else if (bleDirection == 0x0A) drawRightArrow();
    else drawStraightArrow();
  }
  drawDistanceBar(bleDistance);
  display.display();
}

void drawBattery(float voltage) {
  int percent = (voltage - 3.3) * 100;
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  display.setCursor(100, 0);
  display.setTextSize(1);
  display.print("🔋");
  display.print(percent);
  display.print("%");
}

void fetchWeather() {
  if (WiFi.status() == WL_CONNECTED && millis() - lastWeatherUpdate > 300000) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + clockLocation + "&appid=" + weatherApiKey + "&units=metric";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      currentTemp = doc["main"]["temp"];
      String condition = doc["weather"][0]["main"].as<String>();
      if (condition == "Rain") weatherIcon = "🌧";
      else if (condition == "Clouds") weatherIcon = "☁";
      else if (condition == "Clear") weatherIcon = "☀";
      else weatherIcon = "🌤";
      lastWeatherUpdate = millis();
    }
    http.end();
  }
}

void runOledClock() {
  fetchWeather();
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);

  display.clearDisplay();
  drawBattery(voltage);
  display.setTextSize(2);
  int x = (SCREEN_WIDTH - strlen(timeStr) * 12) / 2;
  display.setCursor(x, 16);
  display.print(timeStr);

  display.setTextSize(1);
  x = (SCREEN_WIDTH - strlen(dateStr) * 6) / 2;
  display.setCursor(x, 40);
  display.print(dateStr);

  display.setCursor(0, 56);
  display.print(weatherIcon);
  display.print(" ");
  display.print(currentTemp, 1);
  display.print("C");

  display.display();
}



// ================================================= PHẦN 2/2 =================================================
// ------------------ PHẦN HIỂN THỊ ANIMATION 772 FRAME ------------------
// Chỉ cần include đúng theo tên file frame_00000.h, frame_00001.h, ..., frame_00771.h
// Ví dụ vài dòng đầu:
#include "frames/frame_00000.h"
#include "frames/frame_00001.h"
// ...
#include "frames/frame_00771.h"

const uint8_t* frames[] = {
  frame_00000, frame_00001, /* ..., */ frame_00771
};

void runGIF60() {
  display.clearDisplay();
  display.drawBitmap(0, 0, frames[frameIndex], 128, 64, SSD1306_WHITE);
  display.display();
  frameIndex = (frameIndex + 1) % totalFrames;
  delay(gifDelay.toInt() * 10);
}

// ------------------ WEB GIAO DIỆN & XỬ LÝ CẤU HÌNH ------------------

void handleRoot() {
  String html = "<html><body><h2>MACH Control Panel</h2>";
  html += "<form action='/clock' method='POST'>";
  html += "<label>Vị trí thời tiết:</label><input name='clockLocation' value='" + clockLocation + "'><br>";
  html += "<input type='submit' value='Lưu'>";
  html += "</form><br><a href='/'>Trang chủ</a></body></html>";
  server.send(200, "text/html", html);
}

void handleClockSave() {
  if (server.hasArg("clockLocation")) {
    clockLocation = server.arg("clockLocation");
    prefs.begin("config", false);
    prefs.putString("clockLocation", clockLocation);
    prefs.end();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ------------------ SETUP & LOOP ------------------

void setup() {
  Serial.begin(115200);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  showWelcome();

  WiFi.softAP(ap_ssid, ap_password);
  prefs.begin("config", true);
  gifSpeed = prefs.getString("gifSpeed", "5");
  gifDelay = prefs.getString("gifDelay", "2");
  galaxyOnly = prefs.getBool("galaxyOnly", false);
  gifEnable = prefs.getBool("gifEnable", true);
  clockLocation = prefs.getString("clockLocation", "Thu Duc");
  prefs.end();

  configTime(7 * 3600, 0, "pool.ntp.org");

  server.on("/", handleRoot);
  server.on("/clock", HTTP_POST, handleClockSave);
  server.begin();

  BLEDevice::init("MACH-HUD");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pIndicateCharacteristic = pService->createCharacteristic(
    CHAR_INDICATE_UUID, BLECharacteristic::PROPERTY_INDICATE);
  pIndicateCharacteristic->addDescriptor(new BLE2902());
  pWriteCharacteristic = pService->createCharacteristic(
    CHAR_WRITE_UUID, BLECharacteristic::PROPERTY_WRITE);
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
}

void loop() {
  server.handleClient();

  if (bleConnected && bleHasData) {
    drawHUD();
  } else if (galaxyOnly) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(30, 28);
    display.print("GALAXY");
    display.display();
    delay(300);
  } else if (gifEnable) {
    runGIF60();
  } else {
    runOledClock();
  }
}
