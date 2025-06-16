// Đã cập nhật giao diện theo bố cục HUD: giờ VN, mũi tên, khoảng cách, tên đường chạy chữ, hành trình, tiến trình
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "time.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_I2C_ADDRESS 0x3C
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SERVICE_UUID        "DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_INDICATE_UUID  "DD3F0AD2-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_WRITE_UUID     "DD3F0AD3-6239-4E1F-81F1-91F6C9F01D86"

BLECharacteristic *pWriteCharacteristic;
bool deviceConnected = false;
uint32_t initialDistance = 0;
uint32_t currentDistance = 0;
uint8_t directionCode = 0;
unsigned long lastBlink = 0;
bool blinkState = true;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

String roadName = "D. Pham Van Dong";
String remainKM = "7.3 Km";
String remainMin = "20 min";

void drawArrow(uint8_t dir, bool show) {
  if (!show) return;
  switch (dir) {
    case 0x04: display.fillTriangle(63, 2, 47, 14, 79, 14, SH110X_WHITE); display.fillRect(58, 10, 11, 19, SH110X_WHITE); break;
    case 0x08: display.fillTriangle(32, 10, 45, 0, 45, 20, SH110X_WHITE); display.fillRect(45, 5, 13, 10, SH110X_WHITE); display.fillRect(58, 5, 11, 25, SH110X_WHITE); break;
    case 0x0A: display.fillTriangle(95, 10, 82, 0, 82, 20, SH110X_WHITE); display.fillRect(69, 5, 13, 10, SH110X_WHITE); display.fillRect(58, 5, 11, 25, SH110X_WHITE); break;
    default: break;
  }
}

void scrollText(const String& text, int y, int frame) {
  int w = text.length() * 6;
  int x = SCREEN_WIDTH - (frame % (w + SCREEN_WIDTH));
  display.setCursor(x, y);
  display.print(text);
}

void drawHUD() {
  display.clearDisplay();
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    display.setCursor(SCREEN_WIDTH - 6 * 8, 0);
    display.setTextSize(1);
    display.print(timeStr);
  }

  if (millis() - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = millis();
  }
  drawArrow(directionCode, currentDistance <= 30 ? blinkState : true);

  display.setTextSize(1);
  display.setCursor((SCREEN_WIDTH - 6 * 5) / 2, 36);
  display.print(String(currentDistance) + " m");

  scrollText(roadName, 46, millis() / 100);
  display.setCursor(30, 56);
  display.print(remainKM + "  " + remainMin);

  int barWidth = map(initialDistance - currentDistance, 0, initialDistance, 0, SCREEN_WIDTH - 4);
  display.drawRect(2, SCREEN_HEIGHT - 6, SCREEN_WIDTH - 4, 4, SH110X_WHITE);
  display.fillRect(3, SCREEN_HEIGHT - 5, barWidth, 2, SH110X_WHITE);

  display.display();
}

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) { deviceConnected = false; BLEDevice::startAdvertising(); }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() >= 7 && value[0] == 0x01) {
      directionCode = value[2];
      std::string dstr = value.substr(3, 3);
      currentDistance = atoi(dstr.c_str());
      if (initialDistance == 0 || currentDistance > initialDistance)
        initialDistance = currentDistance;
    }
  }
};

void setup() {
  Serial.begin(115200);
  display.begin(OLED_I2C_ADDRESS, true);
  display.clearDisplay();
  display.display();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  BLEDevice::init("ESP32_Sygic_HUD");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pIndicateCharacteristic = pService->createCharacteristic(
    CHAR_INDICATE_UUID, BLECharacteristic::PROPERTY_INDICATE);
  pIndicateCharacteristic->addDescriptor(new BLE2902());

  pWriteCharacteristic = pService->createCharacteristic(
    CHAR_WRITE_UUID, BLECharacteristic::PROPERTY_WRITE);
  pWriteCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
}

void loop() {
  drawHUD();
  delay(100);
}
