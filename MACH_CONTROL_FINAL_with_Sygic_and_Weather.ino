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

#include "frames/frame_00000.h"
#include "frames/frame_00001.h"
#include "frames/frame_00002.h"
#include "frames/frame_00003.h"
#include "frames/frame_00004.h"
#include "frames/frame_00005.h"
#include "frames/frame_00006.h"
#include "frames/frame_00007.h"
#include "frames/frame_00008.h"
#include "frames/frame_00009.h"
#include "frames/frame_00010.h"
#include "frames/frame_00011.h"
#include "frames/frame_00012.h"
#include "frames/frame_00013.h"
#include "frames/frame_00014.h"
#include "frames/frame_00015.h"
#include "frames/frame_00016.h"
#include "frames/frame_00017.h"
#include "frames/frame_00018.h"
#include "frames/frame_00019.h"
#include "frames/frame_00020.h"
#include "frames/frame_00021.h"
#include "frames/frame_00022.h"
#include "frames/frame_00023.h"
#include "frames/frame_00024.h"
#include "frames/frame_00025.h"
#include "frames/frame_00026.h"
#include "frames/frame_00027.h"
#include "frames/frame_00028.h"
#include "frames/frame_00029.h"
#include "frames/frame_00030.h"
#include "frames/frame_00031.h"
#include "frames/frame_00032.h"
#include "frames/frame_00033.h"
#include "frames/frame_00034.h"
#include "frames/frame_00035.h"
#include "frames/frame_00036.h"
#include "frames/frame_00037.h"
#include "frames/frame_00038.h"
#include "frames/frame_00039.h"
#include "frames/frame_00040.h"
#include "frames/frame_00041.h"
#include "frames/frame_00042.h"
#include "frames/frame_00043.h"
#include "frames/frame_00044.h"
#include "frames/frame_00045.h"
#include "frames/frame_00046.h"
#include "frames/frame_00047.h"
#include "frames/frame_00048.h"
#include "frames/frame_00049.h"
#include "frames/frame_00050.h"
#include "frames/frame_00051.h"
#include "frames/frame_00052.h"
#include "frames/frame_00053.h"
#include "frames/frame_00054.h"
#include "frames/frame_00055.h"
#include "frames/frame_00056.h"
#include "frames/frame_00057.h"
#include "frames/frame_00058.h"
#include "frames/frame_00059.h"
#include "frames/frame_00060.h"
#include "frames/frame_00061.h"
#include "frames/frame_00062.h"
#include "frames/frame_00063.h"
#include "frames/frame_00064.h"
#include "frames/frame_00065.h"
#include "frames/frame_00066.h"
#include "frames/frame_00067.h"
#include "frames/frame_00068.h"
#include "frames/frame_00069.h"
#include "frames/frame_00070.h"
#include "frames/frame_00071.h"
#include "frames/frame_00072.h"
#include "frames/frame_00073.h"
#include "frames/frame_00074.h"
#include "frames/frame_00075.h"
#include "frames/frame_00076.h"
#include "frames/frame_00077.h"
#include "frames/frame_00078.h"
#include "frames/frame_00079.h"
#include "frames/frame_00080.h"
#include "frames/frame_00081.h"
#include "frames/frame_00082.h"
#include "frames/frame_00083.h"
#include "frames/frame_00084.h"
#include "frames/frame_00085.h"
#include "frames/frame_00086.h"
#include "frames/frame_00087.h"
#include "frames/frame_00088.h"
#include "frames/frame_00089.h"
#include "frames/frame_00090.h"
#include "frames/frame_00091.h"
#include "frames/frame_00092.h"
#include "frames/frame_00093.h"
#include "frames/frame_00094.h"
#include "frames/frame_00095.h"
#include "frames/frame_00096.h"
#include "frames/frame_00097.h"
#include "frames/frame_00098.h"
#include "frames/frame_00099.h"
#include "frames/frame_00100.h"
#include "frames/frame_00101.h"
#include "frames/frame_00102.h"
#include "frames/frame_00103.h"
#include "frames/frame_00104.h"
#include "frames/frame_00105.h"
#include "frames/frame_00106.h"
#include "frames/frame_00107.h"
#include "frames/frame_00108.h"
#include "frames/frame_00109.h"
#include "frames/frame_00110.h"
#include "frames/frame_00111.h"
#include "frames/frame_00112.h"
#include "frames/frame_00113.h"
#include "frames/frame_00114.h"
#include "frames/frame_00115.h"
#include "frames/frame_00116.h"
#include "frames/frame_00117.h"
#include "frames/frame_00118.h"
#include "frames/frame_00119.h"
#include "frames/frame_00120.h"
#include "frames/frame_00121.h"
#include "frames/frame_00122.h"
#include "frames/frame_00123.h"
#include "frames/frame_00124.h"
#include "frames/frame_00125.h"
#include "frames/frame_00126.h"
#include "frames/frame_00127.h"
#include "frames/frame_00128.h"
#include "frames/frame_00129.h"
#include "frames/frame_00130.h"
#include "frames/frame_00131.h"
#include "frames/frame_00132.h"
#include "frames/frame_00133.h"
#include "frames/frame_00134.h"
#include "frames/frame_00135.h"
#include "frames/frame_00136.h"
#include "frames/frame_00137.h"
#include "frames/frame_00138.h"
#include "frames/frame_00139.h"
#include "frames/frame_00140.h"
#include "frames/frame_00141.h"
#include "frames/frame_00142.h"
#include "frames/frame_00143.h"
#include "frames/frame_00144.h"
#include "frames/frame_00145.h"
#include "frames/frame_00146.h"
#include "frames/frame_00147.h"
#include "frames/frame_00148.h"
#include "frames/frame_00149.h"
#include "frames/frame_00150.h"
#include "frames/frame_00151.h"
#include "frames/frame_00152.h"
#include "frames/frame_00153.h"
#include "frames/frame_00154.h"
#include "frames/frame_00155.h"
#include "frames/frame_00156.h"
#include "frames/frame_00157.h"
#include "frames/frame_00158.h"
#include "frames/frame_00159.h"
#include "frames/frame_00160.h"
#include "frames/frame_00161.h"
#include "frames/frame_00162.h"
#include "frames/frame_00163.h"
#include "frames/frame_00164.h"
#include "frames/frame_00165.h"
#include "frames/frame_00166.h"
#include "frames/frame_00167.h"
#include "frames/frame_00168.h"
#include "frames/frame_00169.h"
#include "frames/frame_00170.h"
#include "frames/frame_00171.h"
#include "frames/frame_00172.h"
#include "frames/frame_00173.h"
#include "frames/frame_00174.h"
#include "frames/frame_00175.h"
#include "frames/frame_00176.h"
#include "frames/frame_00177.h"
#include "frames/frame_00178.h"
#include "frames/frame_00179.h"
#include "frames/frame_00180.h"
#include "frames/frame_00181.h"
#include "frames/frame_00182.h"
#include "frames/frame_00183.h"
#include "frames/frame_00184.h"
#include "frames/frame_00185.h"
#include "frames/frame_00186.h"
#include "frames/frame_00187.h"
#include "frames/frame_00188.h"
#include "frames/frame_00189.h"
#include "frames/frame_00190.h"
#include "frames/frame_00191.h"
#include "frames/frame_00192.h"
#include "frames/frame_00193.h"
#include "frames/frame_00194.h"
#include "frames/frame_00195.h"
#include "frames/frame_00196.h"
#include "frames/frame_00197.h"
#include "frames/frame_00198.h"
#include "frames/frame_00199.h"
#include "frames/frame_00200.h"
#include "frames/frame_00201.h"
#include "frames/frame_00202.h"
#include "frames/frame_00203.h"
#include "frames/frame_00204.h"
#include "frames/frame_00205.h"
#include "frames/frame_00206.h"
#include "frames/frame_00207.h"
#include "frames/frame_00208.h"
#include "frames/frame_00209.h"
#include "frames/frame_00210.h"
#include "frames/frame_00211.h"
#include "frames/frame_00212.h"
#include "frames/frame_00213.h"
#include "frames/frame_00214.h"
#include "frames/frame_00215.h"
#include "frames/frame_00216.h"
#include "frames/frame_00217.h"
#include "frames/frame_00218.h"
#include "frames/frame_00219.h"
#include "frames/frame_00220.h"
#include "frames/frame_00221.h"
#include "frames/frame_00222.h"
#include "frames/frame_00223.h"
#include "frames/frame_00224.h"
#include "frames/frame_00225.h"
#include "frames/frame_00226.h"
#include "frames/frame_00227.h"
#include "frames/frame_00228.h"
#include "frames/frame_00229.h"
#include "frames/frame_00230.h"
#include "frames/frame_00231.h"
#include "frames/frame_00232.h"
#include "frames/frame_00233.h"
#include "frames/frame_00234.h"
#include "frames/frame_00235.h"
#include "frames/frame_00236.h"
#include "frames/frame_00237.h"
#include "frames/frame_00238.h"
#include "frames/frame_00239.h"
#include "frames/frame_00240.h"
#include "frames/frame_00241.h"
#include "frames/frame_00242.h"
#include "frames/frame_00243.h"
#include "frames/frame_00244.h"
#include "frames/frame_00245.h"
#include "frames/frame_00246.h"
#include "frames/frame_00247.h"
#include "frames/frame_00248.h"
#include "frames/frame_00249.h"
#include "frames/frame_00250.h"
#include "frames/frame_00251.h"
#include "frames/frame_00252.h"
#include "frames/frame_00253.h"
#include "frames/frame_00254.h"
#include "frames/frame_00255.h"
#include "frames/frame_00256.h"
#include "frames/frame_00257.h"
#include "frames/frame_00258.h"
#include "frames/frame_00259.h"
#include "frames/frame_00260.h"
#include "frames/frame_00261.h"
#include "frames/frame_00262.h"
#include "frames/frame_00263.h"
#include "frames/frame_00264.h"
#include "frames/frame_00265.h"
#include "frames/frame_00266.h"
#include "frames/frame_00267.h"
#include "frames/frame_00268.h"
#include "frames/frame_00269.h"
#include "frames/frame_00270.h"
#include "frames/frame_00271.h"
#include "frames/frame_00272.h"
#include "frames/frame_00273.h"
#include "frames/frame_00274.h"
#include "frames/frame_00275.h"
#include "frames/frame_00276.h"
#include "frames/frame_00277.h"
#include "frames/frame_00278.h"
#include "frames/frame_00279.h"
#include "frames/frame_00280.h"
#include "frames/frame_00281.h"
#include "frames/frame_00282.h"
#include "frames/frame_00283.h"
#include "frames/frame_00284.h"
#include "frames/frame_00285.h"
#include "frames/frame_00286.h"
#include "frames/frame_00287.h"
#include "frames/frame_00288.h"
#include "frames/frame_00289.h"
#include "frames/frame_00290.h"
#include "frames/frame_00291.h"
#include "frames/frame_00292.h"
#include "frames/frame_00293.h"
#include "frames/frame_00294.h"
#include "frames/frame_00295.h"
#include "frames/frame_00296.h"
#include "frames/frame_00297.h"
#include "frames/frame_00298.h"
#include "frames/frame_00299.h"
#include "frames/frame_00300.h"
#include "frames/frame_00301.h"
#include "frames/frame_00302.h"
#include "frames/frame_00303.h"
#include "frames/frame_00304.h"
#include "frames/frame_00305.h"
#include "frames/frame_00306.h"
#include "frames/frame_00307.h"
#include "frames/frame_00308.h"
#include "frames/frame_00309.h"
#include "frames/frame_00310.h"
#include "frames/frame_00311.h"
#include "frames/frame_00312.h"
#include "frames/frame_00313.h"
#include "frames/frame_00314.h"
#include "frames/frame_00315.h"
#include "frames/frame_00316.h"
#include "frames/frame_00317.h"
#include "frames/frame_00318.h"
#include "frames/frame_00319.h"
#include "frames/frame_00320.h"
#include "frames/frame_00321.h"
#include "frames/frame_00322.h"
#include "frames/frame_00323.h"
#include "frames/frame_00324.h"
#include "frames/frame_00325.h"
#include "frames/frame_00326.h"
#include "frames/frame_00327.h"
#include "frames/frame_00328.h"
#include "frames/frame_00329.h"
#include "frames/frame_00330.h"
#include "frames/frame_00331.h"
#include "frames/frame_00332.h"
#include "frames/frame_00333.h"
#include "frames/frame_00334.h"
#include "frames/frame_00335.h"
#include "frames/frame_00336.h"
#include "frames/frame_00337.h"
#include "frames/frame_00338.h"
#include "frames/frame_00339.h"
#include "frames/frame_00340.h"
#include "frames/frame_00341.h"
#include "frames/frame_00342.h"
#include "frames/frame_00343.h"
#include "frames/frame_00344.h"
#include "frames/frame_00345.h"
#include "frames/frame_00346.h"
#include "frames/frame_00347.h"
#include "frames/frame_00348.h"
#include "frames/frame_00349.h"
#include "frames/frame_00350.h"
#include "frames/frame_00351.h"
#include "frames/frame_00352.h"
#include "frames/frame_00353.h"
#include "frames/frame_00354.h"
#include "frames/frame_00355.h"
#include "frames/frame_00356.h"
#include "frames/frame_00357.h"
#include "frames/frame_00358.h"
#include "frames/frame_00359.h"
#include "frames/frame_00360.h"
#include "frames/frame_00361.h"
#include "frames/frame_00362.h"
#include "frames/frame_00363.h"
#include "frames/frame_00364.h"
#include "frames/frame_00365.h"
#include "frames/frame_00366.h"
#include "frames/frame_00367.h"
#include "frames/frame_00368.h"
#include "frames/frame_00369.h"
#include "frames/frame_00370.h"
#include "frames/frame_00371.h"
#include "frames/frame_00372.h"
#include "frames/frame_00373.h"
#include "frames/frame_00374.h"
#include "frames/frame_00375.h"
#include "frames/frame_00376.h"
#include "frames/frame_00377.h"
#include "frames/frame_00378.h"
#include "frames/frame_00379.h"
#include "frames/frame_00380.h"
#include "frames/frame_00381.h"
#include "frames/frame_00382.h"
#include "frames/frame_00383.h"
#include "frames/frame_00384.h"
#include "frames/frame_00385.h"
#include "frames/frame_00386.h"
#include "frames/frame_00387.h"
#include "frames/frame_00388.h"
#include "frames/frame_00389.h"
#include "frames/frame_00390.h"
#include "frames/frame_00391.h"
#include "frames/frame_00392.h"
#include "frames/frame_00393.h"
#include "frames/frame_00394.h"
#include "frames/frame_00395.h"
#include "frames/frame_00396.h"
#include "frames/frame_00397.h"
#include "frames/frame_00398.h"
#include "frames/frame_00399.h"
#include "frames/frame_00400.h"
#include "frames/frame_00401.h"
#include "frames/frame_00402.h"
#include "frames/frame_00403.h"
#include "frames/frame_00404.h"
#include "frames/frame_00405.h"
#include "frames/frame_00406.h"
#include "frames/frame_00407.h"
#include "frames/frame_00408.h"
#include "frames/frame_00409.h"
#include "frames/frame_00410.h"
#include "frames/frame_00411.h"
#include "frames/frame_00412.h"
#include "frames/frame_00413.h"
#include "frames/frame_00414.h"
#include "frames/frame_00415.h"
#include "frames/frame_00416.h"
#include "frames/frame_00417.h"
#include "frames/frame_00418.h"
#include "frames/frame_00419.h"
#include "frames/frame_00420.h"
#include "frames/frame_00421.h"
#include "frames/frame_00422.h"
#include "frames/frame_00423.h"
#include "frames/frame_00424.h"
#include "frames/frame_00425.h"
#include "frames/frame_00426.h"
#include "frames/frame_00427.h"
#include "frames/frame_00428.h"
#include "frames/frame_00429.h"
#include "frames/frame_00430.h"
#include "frames/frame_00431.h"
#include "frames/frame_00432.h"
#include "frames/frame_00433.h"
#include "frames/frame_00434.h"
#include "frames/frame_00435.h"
#include "frames/frame_00436.h"
#include "frames/frame_00437.h"
#include "frames/frame_00438.h"
#include "frames/frame_00439.h"
#include "frames/frame_00440.h"
#include "frames/frame_00441.h"
#include "frames/frame_00442.h"
#include "frames/frame_00443.h"
#include "frames/frame_00444.h"
#include "frames/frame_00445.h"
#include "frames/frame_00446.h"
#include "frames/frame_00447.h"
#include "frames/frame_00448.h"
#include "frames/frame_00449.h"
#include "frames/frame_00450.h"
#include "frames/frame_00451.h"
#include "frames/frame_00452.h"
#include "frames/frame_00453.h"
#include "frames/frame_00454.h"
#include "frames/frame_00455.h"
#include "frames/frame_00456.h"
#include "frames/frame_00457.h"
#include "frames/frame_00458.h"
#include "frames/frame_00459.h"
#include "frames/frame_00460.h"
#include "frames/frame_00461.h"
#include "frames/frame_00462.h"
#include "frames/frame_00463.h"
#include "frames/frame_00464.h"
#include "frames/frame_00465.h"
#include "frames/frame_00466.h"
#include "frames/frame_00467.h"
#include "frames/frame_00468.h"
#include "frames/frame_00469.h"
#include "frames/frame_00470.h"
#include "frames/frame_00471.h"
#include "frames/frame_00472.h"
#include "frames/frame_00473.h"
#include "frames/frame_00474.h"
#include "frames/frame_00475.h"
#include "frames/frame_00476.h"
#include "frames/frame_00477.h"
#include "frames/frame_00478.h"
#include "frames/frame_00479.h"
#include "frames/frame_00480.h"
#include "frames/frame_00481.h"
#include "frames/frame_00482.h"
#include "frames/frame_00483.h"
#include "frames/frame_00484.h"
#include "frames/frame_00485.h"
#include "frames/frame_00486.h"
#include "frames/frame_00487.h"
#include "frames/frame_00488.h"
#include "frames/frame_00489.h"
#include "frames/frame_00490.h"
#include "frames/frame_00491.h"
#include "frames/frame_00492.h"
#include "frames/frame_00493.h"
#include "frames/frame_00494.h"
#include "frames/frame_00495.h"
#include "frames/frame_00496.h"
#include "frames/frame_00497.h"
#include "frames/frame_00498.h"
#include "frames/frame_00499.h"
#include "frames/frame_00500.h"
#include "frames/frame_00501.h"
#include "frames/frame_00502.h"
#include "frames/frame_00503.h"
#include "frames/frame_00504.h"
#include "frames/frame_00505.h"
#include "frames/frame_00506.h"
#include "frames/frame_00507.h"
#include "frames/frame_00508.h"
#include "frames/frame_00509.h"
#include "frames/frame_00510.h"
#include "frames/frame_00511.h"
#include "frames/frame_00512.h"
#include "frames/frame_00513.h"
#include "frames/frame_00514.h"
#include "frames/frame_00515.h"
#include "frames/frame_00516.h"
#include "frames/frame_00517.h"
#include "frames/frame_00518.h"
#include "frames/frame_00519.h"
#include "frames/frame_00520.h"
#include "frames/frame_00521.h"
#include "frames/frame_00522.h"
#include "frames/frame_00523.h"
#include "frames/frame_00524.h"
#include "frames/frame_00525.h"
#include "frames/frame_00526.h"
#include "frames/frame_00527.h"
#include "frames/frame_00528.h"
#include "frames/frame_00529.h"
#include "frames/frame_00530.h"
#include "frames/frame_00531.h"
#include "frames/frame_00532.h"
#include "frames/frame_00533.h"
#include "frames/frame_00534.h"
#include "frames/frame_00535.h"
#include "frames/frame_00536.h"
#include "frames/frame_00537.h"
#include "frames/frame_00538.h"
#include "frames/frame_00539.h"
#include "frames/frame_00540.h"
#include "frames/frame_00541.h"
#include "frames/frame_00542.h"
#include "frames/frame_00543.h"
#include "frames/frame_00544.h"
#include "frames/frame_00545.h"
#include "frames/frame_00546.h"
#include "frames/frame_00547.h"
#include "frames/frame_00548.h"
#include "frames/frame_00549.h"
#include "frames/frame_00550.h"
#include "frames/frame_00551.h"
#include "frames/frame_00552.h"
#include "frames/frame_00553.h"
#include "frames/frame_00554.h"
#include "frames/frame_00555.h"
#include "frames/frame_00556.h"
#include "frames/frame_00557.h"
#include "frames/frame_00558.h"
#include "frames/frame_00559.h"
#include "frames/frame_00560.h"
#include "frames/frame_00561.h"
#include "frames/frame_00562.h"
#include "frames/frame_00563.h"
#include "frames/frame_00564.h"
#include "frames/frame_00565.h"
#include "frames/frame_00566.h"
#include "frames/frame_00567.h"
#include "frames/frame_00568.h"
#include "frames/frame_00569.h"
#include "frames/frame_00570.h"
#include "frames/frame_00571.h"
#include "frames/frame_00572.h"
#include "frames/frame_00573.h"
#include "frames/frame_00574.h"
#include "frames/frame_00575.h"
#include "frames/frame_00576.h"
#include "frames/frame_00577.h"
#include "frames/frame_00578.h"
#include "frames/frame_00579.h"
#include "frames/frame_00580.h"
#include "frames/frame_00581.h"
#include "frames/frame_00582.h"
#include "frames/frame_00583.h"
#include "frames/frame_00584.h"
#include "frames/frame_00585.h"
#include "frames/frame_00586.h"
#include "frames/frame_00587.h"
#include "frames/frame_00588.h"
#include "frames/frame_00589.h"
#include "frames/frame_00590.h"
#include "frames/frame_00591.h"
#include "frames/frame_00592.h"
#include "frames/frame_00593.h"
#include "frames/frame_00594.h"
#include "frames/frame_00595.h"
#include "frames/frame_00596.h"
#include "frames/frame_00597.h"
#include "frames/frame_00598.h"
#include "frames/frame_00599.h"
#include "frames/frame_00600.h"
#include "frames/frame_00601.h"
#include "frames/frame_00602.h"
#include "frames/frame_00603.h"
#include "frames/frame_00604.h"
#include "frames/frame_00605.h"
#include "frames/frame_00606.h"
#include "frames/frame_00607.h"
#include "frames/frame_00608.h"
#include "frames/frame_00609.h"
#include "frames/frame_00610.h"
#include "frames/frame_00611.h"
#include "frames/frame_00612.h"
#include "frames/frame_00613.h"
#include "frames/frame_00614.h"
#include "frames/frame_00615.h"
#include "frames/frame_00616.h"
#include "frames/frame_00617.h"
#include "frames/frame_00618.h"
#include "frames/frame_00619.h"
#include "frames/frame_00620.h"
#include "frames/frame_00621.h"
#include "frames/frame_00622.h"
#include "frames/frame_00623.h"
#include "frames/frame_00624.h"
#include "frames/frame_00625.h"
#include "frames/frame_00626.h"
#include "frames/frame_00627.h"
#include "frames/frame_00628.h"
#include "frames/frame_00629.h"
#include "frames/frame_00630.h"
#include "frames/frame_00631.h"
#include "frames/frame_00632.h"
#include "frames/frame_00633.h"
#include "frames/frame_00634.h"
#include "frames/frame_00635.h"
#include "frames/frame_00636.h"
#include "frames/frame_00637.h"
#include "frames/frame_00638.h"
#include "frames/frame_00639.h"
#include "frames/frame_00640.h"
#include "frames/frame_00641.h"
#include "frames/frame_00642.h"
#include "frames/frame_00643.h"
#include "frames/frame_00644.h"
#include "frames/frame_00645.h"
#include "frames/frame_00646.h"
#include "frames/frame_00647.h"
#include "frames/frame_00648.h"
#include "frames/frame_00649.h"
#include "frames/frame_00650.h"
#include "frames/frame_00651.h"
#include "frames/frame_00652.h"
#include "frames/frame_00653.h"
#include "frames/frame_00654.h"
#include "frames/frame_00655.h"
#include "frames/frame_00656.h"
#include "frames/frame_00657.h"
#include "frames/frame_00658.h"
#include "frames/frame_00659.h"
#include "frames/frame_00660.h"
#include "frames/frame_00661.h"
#include "frames/frame_00662.h"
#include "frames/frame_00663.h"
#include "frames/frame_00664.h"
#include "frames/frame_00665.h"
#include "frames/frame_00666.h"
#include "frames/frame_00667.h"
#include "frames/frame_00668.h"
#include "frames/frame_00669.h"
#include "frames/frame_00670.h"
#include "frames/frame_00671.h"
#include "frames/frame_00672.h"
#include "frames/frame_00673.h"
#include "frames/frame_00674.h"
#include "frames/frame_00675.h"
#include "frames/frame_00676.h"
#include "frames/frame_00677.h"
#include "frames/frame_00678.h"
#include "frames/frame_00679.h"
#include "frames/frame_00680.h"
#include "frames/frame_00681.h"
#include "frames/frame_00682.h"
#include "frames/frame_00683.h"
#include "frames/frame_00684.h"
#include "frames/frame_00685.h"
#include "frames/frame_00686.h"
#include "frames/frame_00687.h"
#include "frames/frame_00688.h"
#include "frames/frame_00689.h"
#include "frames/frame_00690.h"
#include "frames/frame_00691.h"
#include "frames/frame_00692.h"
#include "frames/frame_00693.h"
#include "frames/frame_00694.h"
#include "frames/frame_00695.h"
#include "frames/frame_00696.h"
#include "frames/frame_00697.h"
#include "frames/frame_00698.h"
#include "frames/frame_00699.h"
#include "frames/frame_00700.h"
#include "frames/frame_00701.h"
#include "frames/frame_00702.h"
#include "frames/frame_00703.h"
#include "frames/frame_00704.h"
#include "frames/frame_00705.h"
#include "frames/frame_00706.h"
#include "frames/frame_00707.h"
#include "frames/frame_00708.h"
#include "frames/frame_00709.h"
#include "frames/frame_00710.h"
#include "frames/frame_00711.h"
#include "frames/frame_00712.h"
#include "frames/frame_00713.h"
#include "frames/frame_00714.h"
#include "frames/frame_00715.h"
#include "frames/frame_00716.h"
#include "frames/frame_00717.h"
#include "frames/frame_00718.h"
#include "frames/frame_00719.h"
#include "frames/frame_00720.h"
#include "frames/frame_00721.h"
#include "frames/frame_00722.h"
#include "frames/frame_00723.h"
#include "frames/frame_00724.h"
#include "frames/frame_00725.h"
#include "frames/frame_00726.h"
#include "frames/frame_00727.h"
#include "frames/frame_00728.h"
#include "frames/frame_00729.h"
#include "frames/frame_00730.h"
#include "frames/frame_00731.h"
#include "frames/frame_00732.h"
#include "frames/frame_00733.h"
#include "frames/frame_00734.h"
#include "frames/frame_00735.h"
#include "frames/frame_00736.h"
#include "frames/frame_00737.h"
#include "frames/frame_00738.h"
#include "frames/frame_00739.h"
#include "frames/frame_00740.h"
#include "frames/frame_00741.h"
#include "frames/frame_00742.h"
#include "frames/frame_00743.h"
#include "frames/frame_00744.h"
#include "frames/frame_00745.h"
#include "frames/frame_00746.h"
#include "frames/frame_00747.h"
#include "frames/frame_00748.h"
#include "frames/frame_00749.h"
#include "frames/frame_00750.h"
#include "frames/frame_00751.h"
#include "frames/frame_00752.h"
#include "frames/frame_00753.h"
#include "frames/frame_00754.h"
#include "frames/frame_00755.h"
#include "frames/frame_00756.h"
#include "frames/frame_00757.h"
#include "frames/frame_00758.h"
#include "frames/frame_00759.h"
#include "frames/frame_00760.h"
#include "frames/frame_00761.h"
#include "frames/frame_00762.h"
#include "frames/frame_00763.h"
#include "frames/frame_00764.h"
#include "frames/frame_00765.h"
#include "frames/frame_00766.h"
#include "frames/frame_00767.h"
#include "frames/frame_00768.h"
#include "frames/frame_00769.h"
#include "frames/frame_00770.h"
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
