#ifndef HOME_MENU_H
#define HOME_MENU_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;
extern int menuSelectedAppIndex;
extern const int totalAppsCount;
extern bool lastMenuNextState;
extern bool lastMenuSelectState;

// Link back to hardware pins inside esp2.ino
extern const uint8_t BUTTON_PIN;
extern const uint8_t BTN_NEXT;

enum SystemState;
extern SystemState currentState;

// Application Names Array Pool
const char* appMenuNames[] = {
  "Voice AI Assistant",
  "Binance Portfolio",
  "Hardware Settings",
  "Retro Dino Jumper",
  "Check System Update"
};

void drawHomeMenuGrid() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Header section layout
  display.setCursor(0, 0);
  display.println("SELECT APP SYSTEM");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  
  // Dynamic Viewport Math:
  // We calculate a sliding starting window offset so that the highlighted item
  // always remains visible within the 4 readable lines on screen.
  int windowStartOffset = 0;
  if (menuSelectedAppIndex >= 4) {
    windowStartOffset = menuSelectedAppIndex - 3; // Shift screen window down
  }

  // Only render a max of 4 consecutive visible items from our index window
  for (int i = 0; i < 4; i++) {
    int appIndex = windowStartOffset + i;
    if (appIndex >= totalAppsCount) break; // End of items safety boundary

    int verticalTextOffset = 15 + (i * 12); // Kept comfortable 12px font separation gap
    display.setCursor(0, verticalTextOffset);
    
    if (appIndex == menuSelectedAppIndex) {
      display.print("-> ");
      display.println(appMenuNames[appIndex]);
    } else {
      display.print("   ");
      display.println(appMenuNames[appIndex]);
    }
  }
  
  display.fillRect(0, 62, 128, 2, SSD1306_WHITE); // UI underline wrapper bar
  display.display();
}

void handleHomeMenuLauncher() {
  drawHomeMenuGrid();

  bool nextPressed = (digitalRead(BTN_NEXT) == LOW);
  bool selectPressed = (digitalRead(BUTTON_PIN) == LOW);

  // Navigate Cursor down the app menu tree array
  if (nextPressed && lastMenuNextState == HIGH) {
    delay(50); // debounce
    if (digitalRead(BTN_NEXT) == LOW) {
      menuSelectedAppIndex++;
      if (menuSelectedAppIndex >= totalAppsCount) {
        menuSelectedAppIndex = 0; // wrap back up to item 0
      }
    }
  }
  lastMenuNextState = nextPressed;

  // Execute App State Transition on select confirmation key press
  if (selectPressed && lastMenuSelectState == HIGH) {
    delay(50); // debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      display.clearDisplay();
      display.display();

      if (menuSelectedAppIndex == 0) {
        currentState = (SystemState)3; // STATE_APP_AI
        extern void updateDisplay(String title, String body);
        updateDisplay("READY (AI APP)", "Hold Button (G3)\nto Record...\n\nClick G2 to exit\nback to Menu.");
      } 
      else if (menuSelectedAppIndex == 1) {
        currentState = (SystemState)4; // STATE_APP_BINANCE
      } 
      else if (menuSelectedAppIndex == 2) {
        currentState = (SystemState)5; // STATE_APP_SETTINGS
        extern void initSettingsScreen();
        initSettingsScreen();
      } 
      else if (menuSelectedAppIndex == 3) {
        currentState = (SystemState)6; // STATE_APP_GAME
        extern void initSimpleGame();
        initSimpleGame();
      }
      else if (menuSelectedAppIndex == 4) {
        currentState = (SystemState)7; // STATE_APP_UPDATE
      }
    }
  }
  lastMenuSelectState = selectPressed;
  delay(15);
}

#endif