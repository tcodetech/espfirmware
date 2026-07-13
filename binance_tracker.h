#ifndef BINANCE_TRACKER_H
#define BINANCE_TRACKER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <mbedtls/md.h>

extern Adafruit_SSD1306 display;
extern void setRGB(bool r, bool g, bool b);

const char* binance_api_key = "LNGzHWl9mxf6Ef0t9VklcmMm9NNs8RYS4jWR0BMBK9DirEoeSNTpsyu2d8bpGFUt";
const char* binance_secret_key = "0nYUlzxBVZZNbxzXZBr9jXL8P70yHjAI4WPBzzPudEsYSOCOKo86BjueKdSVM0Jd";

static unsigned long lastBinanceRun = 0;
const unsigned long binanceInterval = 10000; 

String generateBinanceSignature(String payload, String secret) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*) secret.c_str(), secret.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*) payload.c_str(), payload.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  String signature = "";
  for (int i = 0; i < 32; i++) {
    if (hmacResult[i] < 0x10) signature += "0";
    signature += String(hmacResult[i], HEX);
  }
  return signature;
}

unsigned long long getBinanceTime() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://demo-fapi.binance.com/fapi/v1/time");
  int httpCode = http.GET();
  unsigned long long serverTime = 0;
  if (httpCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    serverTime = doc["serverTime"].as<unsigned long long>();
  }
  http.end();
  return serverTime;
}

void handleBinanceTracker() {
  if (millis() - lastBinanceRun >= binanceInterval || lastBinanceRun == 0) {
    lastBinanceRun = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      setRGB(true, false, false);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("BINANCE TRACKER");
      display.println("---------------------");
      display.println("\n[ERROR]\nWiFi Disconnected!");
      display.display();
      return;
    }

    unsigned long long serverTime = getBinanceTime();
    if (serverTime == 0) return;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    
    String queryString = "timestamp=" + String(serverTime);
    String signature = generateBinanceSignature(queryString, String(binance_secret_key));
    String url = "https://demo-fapi.binance.com/fapi/v3/positionRisk?" + queryString + "&signature=" + signature;
    
    http.begin(client, url);
    http.addHeader("X-MBX-APIKEY", binance_api_key);
    
    int httpCode = http.GET();
    if (httpCode == 200) {
      String rawPayload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, rawPayload);
      
      if (!error) {
        JsonArray positions = doc.as<JsonArray>();
        
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("BINANCE POSITIONS");
        display.println("---------------------");
        
        int count = 0;
        int verticalOffset = 16;
        double netPnL = 0.0; // Cumulative balance

        for (JsonObject pos : positions) {
          double position_amt = pos["positionAmt"].as<double>();
          if (position_amt != 0.0) {
            count++;
            String symbol = pos["symbol"].as<String>();
            double entry_price = pos["entryPrice"].as<double>();
            double mark_price = pos["markPrice"].as<double>();
            
            double live_pnl = (position_amt > 0) ? (mark_price - entry_price) * abs(position_amt) 
                                                 : (entry_price - mark_price) * abs(position_amt);
            
            netPnL += live_pnl;

            display.setCursor(0, verticalOffset);
            display.print(symbol.substring(0, 5));
            display.print(position_amt > 0 ? " L  $" : " S  $");
            display.print(live_pnl, 1);
            
            verticalOffset += 10;
            if (count >= 4) break; 
          }
        }

        if (count > 0) {
          if (netPnL > 0.0) {
            setRGB(false, true, false); // Green glow if overall profile is in profit
          } else if (netPnL < 0.0) {
            setRGB(true, false, false); // Red glow if you run a net deficit
          } else {
            setRGB(false, false, true); // Blue neutral state accent
          }
        } else {
          display.setCursor(0, 30);
          display.println("No Active Positions");
          setRGB(true, true, true); // White indicator for a clean connected slate with no trades
        }
        display.display();
      }
    }
    http.end();
  }
}

#endif
