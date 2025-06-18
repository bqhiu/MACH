////////////////////////////////////// PHẦN 1: Khởi tạo & WebServer (setup() + loop() khung chính) /////////////////////////////////////////////////
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


////////////////////////////////////// PHẦN 2: HTML giao diện các trang cấu hình (GIF, Clock, WiFi, About, Reset) /////////////////////////
// Trang chủ (nút điều hướng)
const char* HOME_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>MACH Control</title>
<style>body{background:#111;color:#fff;text-align:center;font-family:sans-serif;}
a.button{display:inline-block;padding:10px 20px;margin:10px;background:#333;color:#fff;text-decoration:none;border-radius:5px;}
</style></head><body>
<h2>⚙️ MACH Control Panel</h2>
<a href='/gif' class='button'>🎞 Cấu hình GIF</a>
<a href='/clock' class='button'>⏰ Đồng hồ</a>
<a href='/wifi' class='button'>📶 WiFi</a>
<a href='/about' class='button'>ℹ️ Nhà phát hành</a>
<a href='/reset' class='button'>🧹 Đặt lại</a>
</body></html>
)rawliteral";

// GIF PAGE
const char* GIF_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>GIF Config</title>
<style>body{background:#111;color:#fff;text-align:center;font-family:sans-serif;}
input[type=range]{width:80%;}</style>
</head><body>
<h3>🎞 Cấu hình GIF</h3>
<p>Tốc độ GIF: <input id='gifSpeed' type='range' min='1' max='10' value='5'></p>
<p>Delay đổi GIF: <input id='gifDelay' type='range' min='0' max='10' value='2'></p>
<p>Chế độ GalaxyOnly: <input id='galaxyOnly' type='checkbox'></p>
<p>Bật 60 hoạt ảnh: <input id='gifEnable' type='checkbox' checked></p>
<button onclick='saveGIF()'>💾 Lưu</button><br><br>
<a href='/' class='button'>↩️ Quay về</a>
<script>
function saveGIF(){
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({
    gifSpeed:document.getElementById('gifSpeed').value,
    gifDelay:document.getElementById('gifDelay').value,
    galaxyOnly:document.getElementById('galaxyOnly').checked,
    gifEnable:document.getElementById('gifEnable').checked
  })}).then(()=>alert('Đã lưu!'));
}
</script>
</body></html>
)rawliteral";

// Clock PAGE
const char* CLOCK_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Clock</title>
<style>body{background:#111;color:#fff;text-align:center;font-family:sans-serif;}</style>
</head><body>
<h3>⏰ Cấu hình Oled Clock</h3>
<p>Vị trí thời tiết: <input id='clockLocation' type='text' value='Thu Duc, Ho Chi Minh'></p>
<button onclick='saveClock()'>💾 Lưu</button><br><br>
<a href='/' class='button'>↩️ Quay về</a>
<script>
function saveClock(){
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({
    clockLocation:document.getElementById('clockLocation').value
  })}).then(()=>alert('Đã lưu!'));
}
</script>
</body></html>
)rawliteral";

// WiFi PAGE
const char* WIFI_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>WiFi</title>
<style>body{background:#111;color:#fff;text-align:center;font-family:sans-serif;}</style>
</head><body>
<h3>📶 Cấu hình WiFi</h3>
<p>SSID: <input id='wifiSsid' type='text'></p>
<p>Mật khẩu: <input id='wifiPass' type='password'></p>
<button onclick='saveWiFi()'>💾 Lưu</button><br><br>
<a href='/' class='button'>↩️ Quay về</a>
<script>
function saveWiFi(){
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({
    wifiSsid:document.getElementById('wifiSsid').value,
    wifiPass:document.getElementById('wifiPass').value
  })}).then(()=>alert('Đã lưu!'));
}
</script>
</body></html>
)rawliteral";

// About +  Reset PAGE
const char* ABOUT_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>About</title>
<style>body{background:#111;color:#fff;text-align:center;font-family:sans-serif;}</style>
</head><body>
<h3>🏷 Nhà phát hành</h3>
<p><b>Bùi Quang Hiệu</b></p>
<p>MB Bank: 9999.8888.2002</p>
<a href='/' class='button'>↩️ Quay về</a>
</body></html>
)rawliteral";

const char* RESET_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Reset</title>
<style>body{background:#111;color:#fff;text-align:center;font-family:sans-serif;}</style>
</head><body>
<h3>🧹 Đặt lại toàn bộ cấu hình</h3>
<p>Thiết bị sẽ trở về mặc định sau vài giây...</p>
<a href='/' class='button'>↩️ Quay về</a>
</body></html>
)rawliteral";


/////////////////////////////////////////////// PHẦN 3 – Các hàm xử lý trang Web & lưu cấu hình AJAX ///////////////////////////////////
// Xử lý các route trang Web
void handleRoot() {
  server.send(200, "text/html", HOME_PAGE);
}

void handleGIFPage() {
  server.send(200, "text/html", GIF_PAGE);
}

void handleClockPage() {
  server.send(200, "text/html", CLOCK_PAGE);
}

void handleWiFiPage() {
  server.send(200, "text/html", WIFI_PAGE);
}

void handleAboutPage() {
  server.send(200, "text/html", ABOUT_PAGE);
}

void handleResetPage() {
  prefs.begin("config", false);
  prefs.clear();
  prefs.end();
  server.send(200, "text/html", RESET_PAGE);
  delay(1000);
  ESP.restart();
}

// Nhận dữ liệu AJAX từ /save và lưu vào Preferences
#include <ArduinoJson.h>

void handleSaveConfig() {
  if (server.method() == HTTP_POST) {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, server.arg(0));
    if (!err) {
      prefs.begin("config", false);

      if (doc.containsKey("gifSpeed")) {
        gifSpeed = doc["gifSpeed"].as<String>();
        prefs.putString("gifSpeed", gifSpeed);
      }

      if (doc.containsKey("gifDelay")) {
        gifDelay = doc["gifDelay"].as<String>();
        prefs.putString("gifDelay", gifDelay);
      }

      if (doc.containsKey("galaxyOnly")) {
        galaxyOnly = doc["galaxyOnly"];
        prefs.putBool("galaxyOnly", galaxyOnly);
      }

      if (doc.containsKey("gifEnable")) {
        gifEnable = doc["gifEnable"];
        prefs.putBool("gifEnable", gifEnable);
      }

      if (doc.containsKey("clockLocation")) {
        clockLocation = doc["clockLocation"].as<String>();
        prefs.putString("clockLocation", clockLocation);
      }

      if (doc.containsKey("wifiSsid")) {
        prefs.putString("wifiSsid", doc["wifiSsid"].as<String>());
      }

      if (doc.containsKey("wifiPass")) {
        prefs.putString("wifiPass", doc["wifiPass"].as<String>());
      }

      prefs.end();
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Bad Request");
}


/////////////////////////////////////////// PHẦN 4 – Các hàm giả lập animation (HUD Clock, GIF60, GalaxyDisplay) /////////////////////////////////
// runGalaxyDisplay() – Hành tinh quay giả lập
void runGalaxyDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.print("GALAXY 🌌");
  display.display();
  delay(300);
}

// runGIF60() – Giả lập ảnh GIF đang phát
void runGIF60() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(5, 10);
  display.print("GIF Player");
  display.setCursor(5, 25);
  display.print("Speed: " + gifSpeed);
  display.setCursor(5, 40);
  display.print("Delay: " + gifDelay);
  display.display();

  delay(gifDelay.toInt() * 100); // delay giả lập frame
}

// runOledClock() – HUD đồng hồ OLED đơn giản
void runOledClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char buf[20];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  String timeStr = String(buf);

  strftime(buf, sizeof(buf), "%d/%m/%Y", &timeinfo);
  String dateStr = String(buf);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((128 - timeStr.length() * 12) / 2, 10);
  display.print(timeStr);

  display.setTextSize(1);
  display.setCursor((128 - dateStr.length() * 6) / 2, 40);
  display.print(dateStr);

  display.setCursor(0, 56);
  display.print(clockLocation);
  display.display();

  delay(500);
}
//////////////////////////////////////////////////////////////////////////// END /////////////////////////////////////////////////////////
