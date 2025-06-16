#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "all_frames.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int frameDelay = 33; // ~30fps

void showWelcome() {
  display.clearDisplay();
  display.setTextSize(2); // Cỡ chữ
  display.setTextColor(SSD1306_WHITE);

  // Hiển thị từng chữ 'hello' kiểu viết tay mô phỏng
  String text = "hello";
  for (int i = 0; i < text.length(); i++) {
    display.setCursor(20, 20);
    display.print(text.substring(0, i + 1));
    display.display();
    delay(300); // hiệu ứng từng chữ
    display.clearDisplay();
  }

  // Hiển thị chữ 'hello' hoàn chỉnh và giữ lại
  display.setCursor(20, 20);
  display.print("hello");
  display.display();
  delay(1000);

  // Hiển thị BQH ma mị (nhấp nháy)
  for (int i = 0; i < 3; i++) {
    display.clearDisplay();
    display.setCursor(40, 45);
    display.setTextSize(1);
    display.print("BQH");
    display.display();
    delay(250);

    display.clearDisplay();
    display.display();
    delay(250);
  }

  display.clearDisplay();
  display.display();
}

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.display();

  // Gọi màn hình chào
  showWelcome();
}

void loop() {
  for (int i = 0; i < NUM_FRAMES; i++) {
    display.clearDisplay();
    display.drawBitmap(0, 0, all_frames[i], 128, 64, SSD1306_WHITE);
    display.display();
    delay(frameDelay);
  }
}
