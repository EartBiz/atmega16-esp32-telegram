#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"

#ifndef AVR_RX_PIN
#define AVR_RX_PIN 16
#endif
#ifndef AVR_TX_PIN
#define AVR_TX_PIN 17
#endif
#ifndef LED_PIN
#define LED_PIN 2
#endif

HardwareSerial AvrSerial(2);

String lineBuf;
unsigned long lastSendMs = 0;
unsigned long lastWifiRetryMs = 0;
const unsigned long COOLDOWN_MS = 1200;
const unsigned long WIFI_RETRY_MS = 15000;

static String urlEncode(const String &s) {
  String out;
  out.reserve(s.length() * 2);
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else if (c == ' ') {
      out += '+';
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

bool sendTelegram(const String &text) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN + "/sendMessage";
  if (!https.begin(client, url)) {
    return false;
  }
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String body = "chat_id=" + String(CHAT_ID) + "&text=" + urlEncode(text);
  int code = https.POST(body);
  https.end();
  return code > 0 && code < 400;
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
}

void handleLine(const String &raw) {
  String s = raw;
  s.trim();
  if (s.length() == 0) {
    return;
  }
  Serial.print(F("AVR> "));
  Serial.println(s);

  unsigned long now = millis();
  if (s == "BTN" && (now - lastSendMs) > COOLDOWN_MS) {
    lastSendMs = now;
    digitalWrite(LED_PIN, LOW);
    bool ok = sendTelegram("ATmega16 button -> ESP32 -> Telegram");
    Serial.println(ok ? F("Telegram OK") : F("Telegram FAIL"));
    digitalWrite(LED_PIN, HIGH);
  } else if (s.startsWith("AVR")) {
    sendTelegram(String("Link up: ") + s);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);
  delay(200);
  Serial.println(F("ESP32 UART bridge (ATmega16 -> Telegram)"));

  AvrSerial.begin(9600, SERIAL_8N1, AVR_RX_PIN, AVR_TX_PIN);
  lineBuf.reserve(80);

  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
    sendTelegram(String("ESP32 bridge online: ") + WiFi.localIP().toString());
  } else {
    Serial.println(F("Wi-Fi failed — will retry"));
  }
}

void loop() {
  while (AvrSerial.available()) {
    char c = (char)AvrSerial.read();
    if (c == '\n' || c == '\r') {
      if (lineBuf.length()) {
        handleLine(lineBuf);
        lineBuf = "";
      }
    } else if (lineBuf.length() < 80) {
      lineBuf += c;
    } else {
      lineBuf = "";
    }
  }

  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED && (now - lastWifiRetryMs) > WIFI_RETRY_MS) {
    lastWifiRetryMs = now;
    Serial.println(F("Wi-Fi retry..."));
    connectWifi();
  }
}