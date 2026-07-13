#ifndef SIMPLE_GAME_H
#define BINANCE_TRACKER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;
extern void setRGB(bool r, bool g, bool b);

// Core Engine Physics Parameter Maps
static const int groundY = 56;
static int playerY = 46;
static int playerVelocityY = 0;
static bool isJumping = false;

static int obstacleX = 128;
static int obstacleWidth = 6;
static int obstacleHeight = 10;
static float gameSpeed = 3.5;

static int gameScore = 0;
static bool isGameOver = false;
static unsigned long lastGameFrameUpdate = 0;

void initSimpleGame() {
  playerY = groundY - 10; // Player is a 8x10 box sitting on the floor
  playerVelocityY = 0;
  isJumping = false;
  obstacleX = 128;
  gameSpeed = 3.5;
  gameScore = 0;
  isGameOver = false;
  lastGameFrameUpdate = millis();
  
  setRGB(false, false, true); // Solid blue state light background for game environment mode
}

void handleSimpleGame() {
  // Game Over Validation Monitor
  if (isGameOver) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println("GAME OVER");
    
    display.setTextSize(1);
    display.setCursor(10, 36);
    display.print("Final Score: ");
    display.println(gameScore);
    display.setCursor(10, 50);
    display.println("Press G3 to Retry...");
    display.display();
    
    setRGB(true, false, false); // Flash Red on death
    
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(200); // debounce window
      initSimpleGame();
    }
    return;
  }

  // Enforce Framerate Cap (Runs updates roughly every 30ms)
  if (millis() - lastGameFrameUpdate >= 30) {
    lastGameFrameUpdate = millis();

    // Check button input to initiate a jump
    if (digitalRead(BUTTON_PIN) == LOW && !isJumping) {
      playerVelocityY = -7; // Jump impulse force vector
      isJumping = true;
    }

    // Apply basic Gravity Physics
    playerY += playerVelocityY;
    if (isJumping) {
      playerVelocityY += 1; // Gravity pull vector weight increment
    }

    // Ground collision lock boundary check
    if (playerY >= groundY - 10) {
      playerY = groundY - 10;
      playerVelocityY = 0;
      isJumping = false;
    }

    // Obstacle Translation Physics Movement
    obstacleX -= (int)gameSpeed;
    if (obstacleX < -obstacleWidth) {
      obstacleX = 128; // Spawn obstacle back to the right margin loop edge
      gameScore++;
      
      // Gradually scale game speed difficulty curve as scores rise higher
      if (gameSpeed < 7.0) gameSpeed += 0.2;
    }

    // Bounding Box Collision Detection Calculations
    // Player width is 8 pixels (X: 16 to 24), height is 10 pixels
    bool matchX = (obstacleX <= 24 && obstacleX + obstacleWidth >= 16);
    bool matchY = (playerY + 10 >= groundY - obstacleHeight);

    if (matchX && matchY) {
      isGameOver = true;
      return;
    }

    // Render Frame Canvas
    display.clearDisplay();
    
    // Draw horizontal running base ground floor track
    display.drawLine(0, groundY, 127, groundY, SSD1306_WHITE);
    
    // Render Player Model Box
    display.fillRect(16, playerY, 8, 10, SSD1306_WHITE);
    
    // Render Enemy Obstacle Box
    display.fillRect(obstacleX, groundY - obstacleHeight, obstacleWidth, obstacleHeight, SSD1306_WHITE);
    
    // Print Real-Time Score Board
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(80, 0);
    display.print("Score: ");
    display.print(gameScore);
    
    display.display();
  }
}

#endif
