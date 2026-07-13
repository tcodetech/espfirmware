#ifndef LOADING_H
#define LOADING_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>

extern Adafruit_SSD1306 display;
extern void setRGB(bool r, bool g, bool b);

// LINK STEP: Pull the dynamic version string out from your ota_update.h profile definitions
extern const String CURRENT_FIRMWARE_VERSION; 

#define CENTER_X 64  
#define CENTER_Y 24  
#define RADIUS   18  
#define SCREEN_WIDTH 128

void drawLogoT() { 
  int cx = CENTER_X;  
  int cy = CENTER_Y;  
  display.fillRect(cx - 10, cy - 8, 20, 4, SSD1306_WHITE);
  display.fillRect(cx - 2, cy - 4, 4, 15, SSD1306_WHITE); 
} 

void drawCircleLoading(int angle) { 
  display.clearDisplay();  
  
  // FIX: Shift cursor left to 72 to give "Ver 1.1.1" enough space without forcing line wrapping
  display.setFont(); // Force small standard font style
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(72, 0); 
  display.print("Ver ");
  display.print(CURRENT_FIRMWARE_VERSION); // Dynamically rendering shared system variable instance
  
  const int tailLength = 30;  
  for (int i = 0; i < tailLength; i++) { 
    float a = radians(angle - i * 4);  
    int x = CENTER_X + cos(a) * RADIUS;  
    int y = CENTER_Y + sin(a) * RADIUS;  
    
    if (i == 0) display.drawPixel(x, y, SSD1306_WHITE);  
    else if (i % 2 == 0) display.drawPixel(x, y, SSD1306_WHITE);  
  }  
  
  drawLogoT();

  const char *text = "AI Assistant";
  static uint8_t letters = 0;  
  static unsigned long lastTextUpdate = 0;  
  
  if (letters <= strlen(text) && millis() - lastTextUpdate >= 100) { 
    lastTextUpdate = millis();  
    letters++;  
  }  

  char buffer[20];  
  strncpy(buffer, text, letters);
  buffer[letters] = '\0';

  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(SSD1306_WHITE);
  
  int16_t x1, y1;  
  uint16_t w, h;  
  display.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 60);
  display.print(buffer);
  
  display.setFont(); // Reset font back to default before dropping out
  display.display();  
} 

void runBootAnimation(unsigned long msDuration) { 
  unsigned long startAmt = millis();  
  int angle = 0;  
  while (millis() - startAmt < msDuration) { 
    if (angle < 120) setRGB(true, false, false);
    else if (angle < 240) setRGB(false, true, false);
    else setRGB(false, false, true);

    drawCircleLoading(angle);  
    angle += 6;  
    if (angle >= 360) angle = 0;
    delay(20);
  }  
  setRGB(false, false, false); 
} 

#endif