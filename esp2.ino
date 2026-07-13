#pragma GCC optimize("Os")
#include <WiFi.h>  
#include <WiFiClientSecure.h>  
#include <HTTPClient.h>
#include <ArduinoJson.h>  
#include <driver/i2s_std.h>   
#include <Wire.h>  
#include <Adafruit_GFX.h>  
#include <Adafruit_SSD1306.h>  
#include <Fonts/FreeSansBold9pt7b.h>
#include <Preferences.h>
#include <mbedtls/md.h> 
#include "Audio.h"

// Hardware GPIO Mapping Layout
const uint8_t BUTTON_PIN = 3;   // SELECT / ENTER / Action execution button
const uint8_t BTN_NEXT   = 2;   // MOVE CURSOR down list / Exit running App

// RGB Pin Interface Assignments
#define RGB_RED      4
#define RGB_GREEN    5
#define RGB_BLUE     6

#define OLED_SDA     8  
#define OLED_SCL     9  
#define SCREEN_WIDTH 128  
#define SCREEN_HEIGHT 64  
#define OLED_RESET   -1   

// Global Peripheral Handlers
TwoWire I2COLED = TwoWire(0);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2COLED, OLED_RESET);
Preferences prefs;

// Multi-App Navigation State Enumerations
enum SystemState { STATE_LOADING, STATE_WIFI_PORTAL, STATE_HOME_MENU, STATE_APP_AI, STATE_APP_BINANCE, STATE_APP_SETTINGS, STATE_APP_GAME, STATE_APP_UPDATE };
SystemState currentState = STATE_LOADING;

// Global Menu Tracking Variables
static int menuSelectedAppIndex = 0;
const int totalAppsCount = 5;
static bool lastMenuNextState = HIGH;
static bool lastMenuSelectState = HIGH;

// Universal Hardware Color Driver Helper
void setRGB(bool r, bool g, bool b) {
  digitalWrite(RGB_RED, r ? HIGH : LOW);
  digitalWrite(RGB_GREEN, g ? HIGH : LOW);
  digitalWrite(RGB_BLUE, b ? HIGH : LOW);
}

// Universal Display UI Function Wrapper
void updateDisplay(String title, String body) {  
  display.clearDisplay();  
  display.setTextColor(SSD1306_WHITE);  
  display.setTextSize(1);  
  display.setCursor(0, 0);  
  display.println(title);  
  display.println("---------------------");  
  display.setCursor(0, 16);  
  display.println(body);  
  display.display();  
}  

// Include Application Modules
#include "loading.h"
#include "wifi_portal.h"
#include "home_menu.h"
#include "ai_assistant.h"
#include "binance_tracker.h"
#include "settings_page.h"
#include "simple_game.h"
#include "ota_update.h"

void setup() {  
  Serial.begin(115200);  
  delay(200);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);  
  pinMode(BTN_NEXT, INPUT_PULLUP);
  
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);

  I2COLED.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");  
    for(;;);  
  }  
    
  display.clearDisplay();  
  display.display();  

  // 1. Run Loading Splash Ring
  runBootAnimation(3500); 

  // 2. Initialize Audio Hardware Drivers
  setupAudioAndMic();  
  
  // 3. Preferences Storage Router Connect Validation
  setRGB(false, true, true);
  prefs.begin("wifi", true);
  bool profileFound = false;
  String targetSSID = "";
  String targetPASS = "";

  for (int i = 0; i < 5; i++) {
    String ssidKey = "ssid" + String(i);
    String passKey = "pass" + String(i);
    String savedSSID = prefs.getString(ssidKey.c_str(), "");
    String savedPASS = prefs.getString(passKey.c_str(), "");

    if (savedSSID != "") {
      targetSSID = savedSSID;
      targetPASS = savedPASS;
      profileFound = true;
      break; 
    }
  }
  prefs.end();

  if (profileFound) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.print("Connecting to:\n");
    display.print(targetSSID);
    display.display();

    WiFi.mode(WIFI_STA);
    WiFi.begin(targetSSID.c_str(), targetPASS.c_str());
    
    unsigned long startTimeout = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTimeout < 15000) {
      delay(250);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");
      setRGB(false, false, false);
      
      // Wi-Fi operational -> Launch directly into the new Interactive App Home Menu launcher
      currentState = STATE_HOME_MENU;
      drawHomeMenuGrid();
    } else {
      currentState = STATE_WIFI_PORTAL;
      scanWiFi();
    }
  } else {
    currentState = STATE_WIFI_PORTAL;
    scanWiFi();
  }
}  
  
void loop() {  
  switch(currentState) {
    case STATE_WIFI_PORTAL:
      handleWiFiPortal();
      break;
      
    case STATE_HOME_MENU:
      handleHomeMenuLauncher();
      break;
      
    case STATE_APP_AI:
      // In-App Navigation Command Interrupt Check: Hold/Click G2 to drop back out to Home menu launcher
      if (digitalRead(BTN_NEXT) == LOW) {
        delay(250);
        currentState = STATE_HOME_MENU;
        setRGB(false, false, false);
        drawHomeMenuGrid();
      } else {
        handleAIAssistant();
      }
      break;
      
    case STATE_APP_BINANCE:
      if (digitalRead(BTN_NEXT) == LOW) {
        delay(250);
        currentState = STATE_HOME_MENU;
        setRGB(false, false, false);
        drawHomeMenuGrid();
      } else {
        handleBinanceTracker();
      }
      break;

    case STATE_APP_SETTINGS:
      if (digitalRead(BTN_NEXT) == LOW) {
        delay(250);
        currentState = STATE_HOME_MENU;
        setRGB(false, false, false);
        drawHomeMenuGrid();
      } else {
        handleSettingsPage();
      }
      break;

    case STATE_APP_GAME:
      if (digitalRead(BTN_NEXT) == LOW) {
        delay(250);
        currentState = STATE_HOME_MENU;
        setRGB(false, false, false);
        drawHomeMenuGrid();
      } else {
        handleSimpleGame();
      }
      break;
    case STATE_APP_UPDATE:
      // If user taps BTN_NEXT (G2), cancel/return back out safely into the Main Menu Launcher
      if (digitalRead(BTN_NEXT) == LOW) {
        delay(250);
        currentState = STATE_HOME_MENU;
        setRGB(false, false, false);
        drawHomeMenuGrid();
      } else {
        handleSystemCloudUpdate();
      }
      break;  
      
    default:
      break;
  }
}  
