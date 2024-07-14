#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiMulti wifiMulti;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.jst.mfeed.ad.jp", 9 * 3600); // 日本時間（UTC+9）

AsyncWebServer server(80);

unsigned long startTime = 0;
unsigned long countdownTime = 0;
bool countdownActive = false;

M5Canvas Sprite;

void showConnectingScreen() {
  while (wifiMulti.run() != WL_CONNECTED) {
    Sprite.fillScreen(TFT_BLACK);
    Sprite.setCursor(10, 50);
    Sprite.setTextSize(2);
    Sprite.setTextColor(TFT_WHITE);
    Sprite.print("Connecting...");
    Sprite.pushSprite(&M5.Display, 0, 0);
    delay(500);
  }
}

void setup() {
  M5.begin();
  M5.Display.begin();
  M5.Display.setRotation(0);

  Sprite.createSprite(128, 128);
  Sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  Sprite.setTextSize(2);

  // WiFi設定
  wifiMulti.addAP("SSID1", "PASSWORD1");
//  wifiMulti.addAP("SSID2", "PASSWORD2");
//  wifiMulti.addAP("SSID3", "PASSWORD3");

  Serial.println("Connecting to WiFi...");
  showConnectingScreen();
  Serial.println("\nWiFi connected.");
  Serial.printf("+ SSID : %s\n", WiFi.SSID());
  Serial.printf("+ IP   : %s\n", WiFi.localIP().toString().c_str());

  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  Serial.println("NTP time synchronized.");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", R"rawliteral(
      <!DOCTYPE HTML>
      <html>
      <head>
        <title>Timer Setup</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;700&display=swap" rel="stylesheet">
        <style>
          body { font-family: 'Roboto', sans-serif; font-size: 24px; }
          label { display: block; margin-top: 10px; font-weight: bold; }
          input[type="number"], input[type="text"] { width: 100%; padding: 10px; box-sizing: border-box; font-size: 24px; }
          input[type="submit"] { width: 100%; padding: 10px; margin-top: 20px; background-color: #4CAF50; color: white; border: none; font-size: 24px; cursor: pointer; }
        </style>
      </head>
      <body>
        <h1>Set Timer</h1>
        <form action="/set" method="POST">
          <label for="hours">Hours:</label>
          <input type="number" id="hours" name="hours" min="0" step="1">
          <label for="minutes">Minutes:</label>
          <input type="number" id="minutes" name="minutes" min="0" step="1">
          <label for="seconds">Seconds:</label>
          <input type="number" id="seconds" name="seconds" min="0" step="1">
          <input type="submit" value="Set Timer">
        </form>
        <form action="/set-time" method="POST" style="margin-top:20px;">
          <label for="targetTime">Target Time (HH:MM:SS):</label>
          <input type="text" id="targetTime" name="targetTime" pattern="[0-9]{2}:[0-9]{2}:[0-9]{2}" placeholder="HH:MM:SS">
          <input type="submit" value="Set Timer to Target Time">
        </form>
      </body>
      </html>
    )rawliteral");
  });

  server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request){
    int hours = 0, minutes = 0, seconds = 0;
    if (request->hasParam("hours", true)) {
      hours = request->getParam("hours", true)->value().toInt();
    }
    if (request->hasParam("minutes", true)) {
      minutes = request->getParam("minutes", true)->value().toInt();
    }
    if (request->hasParam("seconds", true)) {
      seconds = request->getParam("seconds", true)->value().toInt();
    }
    countdownTime = (hours * 3600 + minutes * 60 + seconds) * 1000;
    startTime = millis();
    countdownActive = true;
    request->send_P(200, "text/html", R"rawliteral(
      <!DOCTYPE HTML>
      <html>
      <head>
        <title>Timer Set</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;700&display=swap" rel="stylesheet">
        <style>
          body { font-family: 'Roboto', sans-serif; font-size: 24px; text-align: center; margin-top: 20px; }
          p { font-size: 24px; }
          a { font-size: 24px; }
        </style>
      </head>
      <body>
        <h1>Timer Set Successfully</h1>
        <p>The timer has been set.</p>
        <p><a href="/">Go back</a></p>
      </body>
      </html>
    )rawliteral");
  });

  server.on("/set-time", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("targetTime", true)) {
      String targetTime = request->getParam("targetTime", true)->value();
      int targetHours = targetTime.substring(0, 2).toInt();
      int targetMinutes = targetTime.substring(3, 5).toInt();
      int targetSeconds = targetTime.substring(6, 8).toInt();

      timeClient.update();
      time_t currentTime = timeClient.getEpochTime();
      struct tm *currentTimeInfo = localtime(&currentTime);
      int currentHours = currentTimeInfo->tm_hour;
      int currentMinutes = currentTimeInfo->tm_min;
      int currentSeconds = currentTimeInfo->tm_sec;

      int remainingSeconds = (targetHours * 3600 + targetMinutes * 60 + targetSeconds) -
                             (currentHours * 3600 + currentMinutes * 60 + currentSeconds);

      if (remainingSeconds < 0) {
        remainingSeconds += 24 * 3600; // 翌日の時間に設定
      }

      countdownTime = remainingSeconds * 1000;
      startTime = millis();
      countdownActive = true;
    }
    request->send_P(200, "text/html", R"rawliteral(
      <!DOCTYPE HTML>
      <html>
      <head>
        <title>Timer Set</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;700&display=swap" rel="stylesheet">
        <style>
          body { font-family: 'Roboto', sans-serif; font-size: 24px; text-align: center; margin-top: 20px; }
          p { font-size: 24px; }
          a { font-size: 24px; }
        </style>
      </head>
      <body>
        <h1>Timer Set Successfully</h1>
        <p>The timer has been set to the target time.</p>
        <p><a href="/">Go back</a></p>
      </body>
      </html>
    )rawliteral");
  });

  server.begin();
}

void loop() {
  M5.update();  // 本体状態更新
  unsigned long currentMillis = millis();

  Sprite.fillScreen(TFT_BLACK);

  if (countdownActive) {
    unsigned long elapsed = currentMillis - startTime;
    if (elapsed >= countdownTime) {
      countdownActive = false;
      elapsed = countdownTime;
    }

    unsigned long remaining = countdownTime - elapsed;
    unsigned long remainingSeconds = remaining / 1000;
    unsigned long hours = remainingSeconds / 3600;
    remainingSeconds %= 3600;
    unsigned long minutes = remainingSeconds / 60;
    unsigned long seconds = remainingSeconds % 60;

    // 残り時間のバーグラフをブロックで描画
    int totalBlocks = 16;
    int blockWidth = 8;
    int blocksToDraw = max(1, (int)map(remaining, 0, countdownTime, 0, totalBlocks));

    for (int i = 0; i < blocksToDraw; i++) {
      Sprite.fillRect(i * blockWidth, 0, blockWidth - 1, 20, TFT_GREEN);
    }

    Sprite.setCursor(0, 30);
    Sprite.printf("Left:\n%02lu:%02lu:%02lu", hours, minutes, seconds);

    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *timeinfo = localtime(&epochTime);

    Sprite.setCursor(0, 80);
    Sprite.printf("Now:\n%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  } else {
    Sprite.setCursor(0, 20);
    Sprite.println("Timer Set:");
    Sprite.println("" + WiFi.localIP().toString());
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *timeinfo = localtime(&epochTime);

    Sprite.setCursor(0, 80);
    Sprite.printf("Now:\n%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  }
  Sprite.pushSprite(&M5.Display, 0, 0);

  if (M5.BtnA.wasPressed()) {  //ボタンが押されたら
    Serial.println("#### BtnA.wasClicked");
    M5.Display.qrcode("http://" + WiFi.localIP().toString(), 1, 1, 128, 6);
    delay(10000);
  }

  delay(10);
}
