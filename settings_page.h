#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;
extern Preferences prefs;
extern void setRGB(bool r, bool g, bool b);

static int currentVolSetting = 21;
static bool lastSetButtonState = HIGH;

void renderSettingsUI() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Header Row Info
  display.setCursor(0, 0);
  display.println("DEVICE SETTINGS");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  
  // Feature Option 1: Live Hardware Volume Control
  display.setCursor(0, 15);
  display.print("Speaker Vol: [ ");
  display.print(currentVolSetting);
  display.println(" ]");
  
  // Draw an inline progress indicator scale bar for audio level
  display.drawRect(4, 26, 120, 6, SSD1306_WHITE);
  int progressWidth = map(currentVolSetting, 0, 30, 0, 118);
  display.fillRect(5, 27, progressWidth, 4, SSD1306_WHITE);

  // Feature Option 2: Real-time RSSI Signal Status Metrics
  display.setCursor(0, 38);
  display.print("SSID: "); 
  display.println(WiFi.SSID().length() > 0 ? WiFi.SSID().substring(0, 14) : "NONE");
  
  display.setCursor(0, 48);
  display.print("Signal: ");
  long rssi = WiFi.RSSI();
  display.print(rssi);
  display.print(" dBm (");
  if(rssi >= -60) display.print("EXCELLENT)");
  else if(rssi >= -75) display.print("GOOD)");
  else display.print("POOR)");

  display.fillRect(0, 60, 128, 4, SSD1306_WHITE); // Visual footer marker
  display.display();
}

void initSettingsScreen() {
  // Glow a calming White/Blue hybrid status light while reviewing parameters
  setRGB(true, true, true); 
  
  prefs.begin("system", true);
  currentVolSetting = prefs.getInt("volume", 21);
  prefs.end();
  
  renderSettingsUI();
}

void handleSettingsPage() {
  bool currentSelectState = digitalRead(BUTTON_PIN);
  
  // Intercept trigger on tap press release event
  if (lastSetButtonState == HIGH && currentSelectState == LOW) {
    delay(50); // debounce window
    if (digitalRead(BUTTON_PIN) == LOW) {
      
      // Increment option value looping round boundaries (0 to 30 max library window limit)
      currentVolSetting += 3;
      if (currentVolSetting > 30) currentVolSetting = 0;
      
      // Permanently write config profile parameter changes to non-volatile flash slots
      prefs.begin("system", false);
      prefs.putInt("volume", currentVolSetting);
      prefs.end();
      
      // Force immediate peripheral updates on the dynamic object container instances
      extern Audio audio;
      audio.setVolume(currentVolSetting);
      
      // Rapid yellow status indicator blink response upon capture execution validation
      setRGB(true, true, false);
      renderSettingsUI();
      delay(80);
      setRGB(true, true, true);
    }
  }
  lastSetButtonState = currentSelectState;
}

#endif
