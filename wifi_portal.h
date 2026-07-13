#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;
extern Preferences prefs;
extern void updateDisplay(String title, String body);
extern void setRGB(bool r, bool g, bool b);

enum SystemState; 
extern SystemState currentState;

enum UIState { SCAN_WIFI, SELECT_WIFI, PASSWORD_ENTRY, CONNECTING };
static UIState portalState = SCAN_WIFI; 

static String ssidList[20];
static int wifiCount = 0;
static int selected = 0;
static String enteredPassword = "";
static int charIndex = 0;

const char charset[] =
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "@._-!< ";

static bool lastNextState = HIGH;
static bool lastSelectState = HIGH;
static unsigned long selectHoldStart = 0;

void scanWiFi() {
  setRGB(true, true, false); // Cyan status for initialization scanning phase
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 30);
  display.print("Scanning...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(500);

  int n = WiFi.scanNetworks();
  wifiCount = 0;

  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;

    ssidList[wifiCount++] = s;
    if (wifiCount >= 20) break;
  }
  portalState = SELECT_WIFI;
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Select WiFi");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  int start = 0;
  if (selected > 4) start = selected - 4;

  for (int i = 0; i < 5; i++) {
    int idx = start + i;
    if (idx >= wifiCount) break;

    display.setCursor(0, 14 + i * 10);
    if (idx == selected) display.print("> ");
    else display.print("  ");

    String name = ssidList[idx];
    if (name.length() > 17) name = name.substring(0, 17);
    display.print(name);
  }
  display.display();
}

void drawPasswordScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("WiFi:");
  display.println(ssidList[selected]);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 16);
  display.print("Password");
  display.setCursor(0, 28);

  for (int i = 0; i < enteredPassword.length(); i++) {
    display.print("*");
  }
  display.print("_");

  display.setCursor(0, 50);
  char c = charset[charIndex];
  if (c == '<') display.print("Char: DEL");
  else if (c == ' ') display.print("Char: SPACE");
  else {
    display.print("Char: ");
    display.print(c);
  }
  display.display();
}

void handleWiFiPortal() {
  bool next = !digitalRead(BTN_NEXT);
  bool select = !digitalRead(BUTTON_PIN);

  if (portalState == SELECT_WIFI) {
    drawMenu();

    if (next && !lastNextState) {
      selected++;
      if (selected >= wifiCount) selected = 0;
    }

    if (select && !lastSelectState) {
      enteredPassword = "";
      charIndex = 0;
      portalState = PASSWORD_ENTRY;
    }
  } 
  
  else if (portalState == PASSWORD_ENTRY) {
    drawPasswordScreen();

    static unsigned long nextHold = 0;
    static unsigned long lastRepeat = 0;

    if (next) {
      if (nextHold == 0) {
        nextHold = millis();
        charIndex++;
        if (charIndex >= strlen(charset)) charIndex = 0;
        lastRepeat = millis();
      } else {
        unsigned long interval = 200;
        if (millis() - nextHold > 1000) interval = 50; 

        if (millis() - lastRepeat >= interval) {
          lastRepeat = millis();
          charIndex++;
          if (charIndex >= strlen(charset)) charIndex = 0;
        }
      }
    } else {
      nextHold = 0;
      lastRepeat = 0;
    }

    if (select && !lastSelectState) {
      char c = charset[charIndex];
      if (c == '<') {
        if (enteredPassword.length() > 0) {
          enteredPassword.remove(enteredPassword.length() - 1);
        }
      } else {
        enteredPassword += c;
      }
    }

    if (select) {
      if (selectHoldStart == 0) selectHoldStart = millis();

      if (millis() - selectHoldStart > 1500) {
        setRGB(true, true, false); // Keep Cyan lit while authenticating manual entries
        display.clearDisplay();
        display.setCursor(0, 20);
        display.print("Connecting...");
        display.display();

        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true, true);
        delay(500);

        WiFi.begin(ssidList[selected].c_str(), enteredPassword.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
          delay(250);
          Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
          prefs.begin("wifi", false);

          for (int i = 0; i < 5; i++) {
            String oldSsidKey = "ssid" + String(i);
            String oldPassKey = "pass" + String(i);
            prefs.remove(oldSsidKey.c_str());
            prefs.remove(oldPassKey.c_str());
          }

          prefs.putString("ssid0", ssidList[selected]);
          prefs.putString("pass0", enteredPassword);
          prefs.end();

          display.clearDisplay();
          display.setCursor(15, 25);
          display.print("Connected!");
          display.display();
          
          setRGB(false, true, false); // Green flash confirmation
          delay(1000);
          setRGB(false, false, false);

          currentState = (SystemState)2; // Shift state into voice AI app loop
          updateDisplay("READY (AI APP)", "Hold Button (G3)\nto Record...\n\nClick G2 to open\nBinance App ->");
          return;
        } else {
          setRGB(true, false, false); // Red light fault flash
          display.clearDisplay();
          display.setCursor(25, 25);
          display.print("Failed!");
          display.display();
          delay(2000);
          setRGB(false, false, false);
          portalState = PASSWORD_ENTRY;
        }
        selectHoldStart = 0;
      }
    } else {
      selectHoldStart = 0;
    }
  }

  lastNextState = next;
  lastSelectState = select;
  delay(35);
}

#endif
