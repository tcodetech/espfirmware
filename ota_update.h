#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;
extern void setRGB(bool r, bool g, bool b);

// FIX 1: Removed duplicate definition of enum SystemState.
// Since it's already defined in esp2.ino, we just need to forward declare it here:
typedef enum SystemState SystemState; 
extern SystemState currentState;

extern const uint8_t BUTTON_PIN; 
extern const uint8_t BTN_NEXT;   

// FIX 2: Wrapped the version inside double quotes so it's a valid String literal
// Inside ota_update.h around line 18-20:
extern const String CURRENT_FIRMWARE_VERSION; 
const String CURRENT_FIRMWARE_VERSION = "1.2.1"; 
const char* githubApiUrl = "https://api.github.com/repos/tcodetech/espfirmware/releases/latest";

// Helper function to split a string by a delimiter
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// Semantic Version comparison logic: Returns true if remote version is strictly newer than current
bool isNewerVersion(String current, String remote) {
  if (current.startsWith("v")) current = current.substring(1);
  if (remote.startsWith("v")) remote = remote.substring(1);

  for (int i = 0; i < 3; i++) {
    int currChunk = getValue(current, '.', i).toInt();
    int remChunk = getValue(remote, '.', i).toInt();
    
    if (remChunk > currChunk) return true;
    if (remChunk < currChunk) return false;
  }
  return false; 
}

void waitForExitButton() {
  delay(300); 
  while (true) {
    if (digitalRead(BTN_NEXT) == LOW) {
      delay(200); 
      currentState = (SystemState)2; // Hard fallback link index matching STATE_HOME_MENU
      break;
    }
    yield();
  }
}

void handleSystemCloudUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    setRGB(true, false, false);
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SYSTEM UPDATE");
    display.println("---------------------");
    display.println("\n[ERROR]\nNo WiFi Connection!");
    display.println("\nClick G2 to Go Back");
    display.display();
    waitForExitButton();
    return;
  }

  setRGB(true, true, false);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SYSTEM UPDATE");
  display.println("---------------------");
  display.setCursor(0, 20);
  display.println("Checking GitHub API...");
  display.display();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(githubApiUrl);
  http.setUserAgent("ESP32-S3-AI-Assistant"); 

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("API ERROR");
    display.println("---------------------");
    display.printf("\nHTTP Error Code: %d", httpCode);
    display.println("\n\nClick G2 to Go Back");
    display.display();
    http.end();
    waitForExitButton();
    return;
  }

  DynamicJsonDocument doc(4096); 
  DeserializationError error = deserializeJson(doc, http.getStream());
  
  if (error) {
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("PARSING ERROR");
    display.println("---------------------");
    display.println("\nFailed parsing JSON.");
    display.display();
    http.end();
    waitForExitButton();
    return;
  }

  String tag_name = doc["tag_name"].as<String>();
  const char* changelog = doc["body"] | "General updates.";

  if (tag_name == "null" || tag_name.length() == 0) {
    tag_name = "1.0.0";
  }

  String remoteBinaryUrl = "";
  JsonArray assets = doc["assets"];
  for (JsonObject asset : assets) {
    const char* assetName = asset["name"];
    if (String(assetName).endsWith(".bin")) {
      remoteBinaryUrl = asset["browser_download_url"].as<String>();
      break;
    }
  }

  http.end(); 

  if (!isNewerVersion(CURRENT_FIRMWARE_VERSION, tag_name)) {
    setRGB(false, true, false); 
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("SYSTEM UP-TO-DATE");
    display.println("---------------------");
    display.print("\nInstalled: "); display.println(CURRENT_FIRMWARE_VERSION);
    display.print("Latest:    "); display.println(tag_name);
    display.println("\nNo update needed.");
    display.println("\nClick G2 to Go Back");
    display.display();
    waitForExitButton(); 
    return;
  }

  if (remoteBinaryUrl.length() == 0) {
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("ASSET ERROR");
    display.println("---------------------");
    display.println("\nNo .bin asset found!");
    display.println("\nClick G2 to Go Back");
    display.display();
    waitForExitButton();
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("NEW UPDATE FOUND!");
  display.println("---------------------");
  display.print("Version: "); display.println(tag_name);
  display.println(changelog);
  display.setCursor(0, 46);
  display.println(">> G3: Download Bin");
  display.println(">> G2: Cancel/Exit");
  display.display();
  
  delay(300); 
  bool proceedWithDownload = false;

  while (true) {
    if (digitalRead(BUTTON_PIN) == LOW) { 
      delay(200);
      proceedWithDownload = true;
      break;
    }
    if (digitalRead(BTN_NEXT) == LOW) { 
      delay(200);
      currentState = (SystemState)2; // Return to STATE_HOME_MENU
      return;
    }
    yield();
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("SYSTEM UPDATE");
  display.println("---------------------");
  display.setCursor(0, 20);
  display.println("Connecting stream...");
  display.display();

  http.begin(remoteBinaryUrl);
  http.setUserAgent("ESP32-S3-AI-Assistant");
  httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("DOWNLOAD ERROR");
    display.println("---------------------");
    display.printf("\nDownload failed: %d", httpCode);
    display.println("\n\nClick G2 to Go Back");
    display.display();
    http.end();
    waitForExitButton();
    return;
  }

  int contentLength = http.getSize();
  if (!Update.begin(contentLength, U_FLASH)) {
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("SPACE ERROR");
    display.println("---------------------");
    display.println("\nBinary too large\nfor partition!");
    display.println("\nClick G2 to Go Back");
    display.display();
    http.end();
    waitForExitButton();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t writtenBytes = 0;
  uint8_t buffer[512];
  unsigned long lastOledProgressTime = 0;

  while (http.connected() && (writtenBytes < contentLength)) {
    size_t availableBytes = stream->available();
    if (availableBytes > 0) {
      size_t readLen = stream->readBytes(buffer, min(availableBytes, sizeof(buffer)));
      if (readLen > 0) {
        Update.write(buffer, readLen);
        writtenBytes += readLen;
        yield(); 

        if (millis() - lastOledProgressTime > 300) {
          lastOledProgressTime = millis();
          int progressPercent = (writtenBytes * 100) / contentLength;
          
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("FLASHING DEVICE");
          display.println("---------------------");
          display.setCursor(0, 20);
          display.printf("Progress: %d%%\n", progressPercent);
          display.printf("Bytes: %d/%d\n", writtenBytes, contentLength);
          display.println("\nDO NOT POWER OFF!");
          display.display();
        }
      }
    }
    yield();
  }

  if (Update.end(true)) {
    setRGB(false, true, false); 
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("UPDATE SUCCESS");
    display.println("---------------------");
    display.println("\nFlashing Complete!\nRebooting device...");
    display.display();
    delay(2000);
    ESP.restart(); 
  } else {
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("FLASH FAILURE");
    display.println("---------------------");
    display.printf("\nError Code: %d\n", Update.getError());
    display.println("\nClick G2 to Go Back");
    display.display();
    waitForExitButton();
  }
  
  http.end();
}

#endif