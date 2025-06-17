// === PHẦN 1 + 2 + 3 + 4 + ICON + THỜI TIẾT: GIỜ, NGÀY, PIN, NHIỆT ĐỘ VÀ ICON ===
// ESP32-C3 MINI + OLED SSD1306 128x64
// Dùng thư viện Adafruit SSD1306 + GFX + time + HTTPClient

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Cấu hình WiFi và múi giờ VN
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// Chân đọc pin (ADC)
#define PIN_BATTERY A0

// OpenWeatherMap API
const char* OPENWEATHER_API_KEY = "d2386720d2ad0a3823385ec99bd27598"; // Nhập API
const char* weather_url = "http://api.openweathermap.org/data/2.5/weather?lat=10.847&lon=106.769&units=metric&appid=";
String weatherMain = "";

int getBatteryPercent() {
  int raw = analogRead(PIN_BATTERY);
  float voltage = (raw / 4095.0) * 3.3 * 2;
  int percent = map(voltage * 100, 330, 420, 0, 100);
  percent = constrain(percent, 0, 100);
  return percent;
}

String getTemperatureFromAPI() {
  HTTPClient http;
  String fullUrl = String(weather_url) + OPENWEATHER_API_KEY;
  http.begin(fullUrl);
  int httpCode = http.GET();
  String tempResult = "--\xB0C  HoChiMinh";

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);
    float temp = doc["main"]["temp"];
    weatherMain = doc["weather"][0]["main"].as<String>();
    tempResult = String(round(temp)) + "\xB0C  HoChiMinh";
  }
  http.end();
  return tempResult;
}

void drawTemperatureIcon(int x, int y) {
  display.drawCircle(x + 3, y + 3, 3, SSD1306_WHITE);
  display.drawRect(x + 2, y + 6, 3, 6, SSD1306_WHITE);
  display.fillRect(x + 3, y + 9, 1, 2, SSD1306_WHITE);
}

void drawWeatherIcon(String condition, int x, int y) {
  if (condition == "Clear") {
    display.drawCircle(x + 5, y + 5, 4, SSD1306_WHITE);
  } else if (condition == "Clouds") {
    display.fillCircle(x + 4, y + 6, 3, SSD1306_WHITE);
    display.fillCircle(x + 8, y + 6, 3, SSD1306_WHITE);
    display.fillRect(x + 3, y + 6, 7, 4, SSD1306_WHITE);
  } else if (condition == "Rain") {
    display.fillCircle(x + 4, y + 5, 3, SSD1306_WHITE);
    display.fillCircle(x + 8, y + 5, 3, SSD1306_WHITE);
    display.fillRect(x + 3, y + 6, 7, 4, SSD1306_WHITE);
    display.drawLine(x + 5, y + 11, x + 5, y + 13, SSD1306_WHITE);
    display.drawLine(x + 8, y + 11, x + 8, y + 13, SSD1306_WHITE);
  }
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 không tìm thấy"));
    for (;;);
  }
  display.clearDisplay();
  display.display();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Đang đồng bộ thời gian...");
}

void loop() {
  display.clearDisplay();

  // === Điện áp và % pin góc trên cùng ===
  int raw = analogRead(PIN_BATTERY);
  float voltage = (raw / 4095.0) * 3.3 * 2;
  int percent = getBatteryPercent();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(String(voltage, 2) + "V");

  String pinText = String(percent) + "%";
  display.setCursor(SCREEN_WIDTH - pinText.length() * 6 - 8, 0);
  display.print(pinText);

  int bx = SCREEN_WIDTH - 24;
  display.drawRect(bx, 0, 18, 8, SSD1306_WHITE);
  display.drawRect(bx + 18, 2, 2, 4, SSD1306_WHITE);
  int fill = map(percent, 0, 100, 0, 16);
  display.fillRect(bx + 1, 1, fill, 6, SSD1306_WHITE);

  // === Đồng hồ và ngày ===
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    display.setCursor(0, 10);
    display.print("No Time");
    display.display();
    delay(1000);
    return;
  }

  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  display.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 18);
  display.print(timeStr);

  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a %d %b", &timeinfo);
  display.setTextSize(1);
  display.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 40);
  display.print(dateStr);

  // === Nhiệt độ + thời tiết góc dưới ===
  String tempStr = getTemperatureFromAPI();
  drawTemperatureIcon(0, SCREEN_HEIGHT - 11);
  display.setCursor(10, SCREEN_HEIGHT - 8);
  display.print(tempStr);
  drawWeatherIcon(weatherMain, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 14);

  display.display();
  delay(10000);
}
