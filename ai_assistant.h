#ifndef AI_ASSISTANT_H
#define AI_ASSISTANT_H
#pragma GCC optimize("Os")
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "Audio.h"

extern Adafruit_SSD1306 display;
extern Preferences prefs;
extern void updateDisplay(String title, String body);
extern void setRGB(bool r, bool g, bool b);

#define MIC_SCK           13    
#define MIC_WS            12    
#define I2S_SD            11    
#define SAMPLE_RATE       16000  
#define MAX_RECORD_SAMPLES (16000 * 6)

#define SPK_SCK           7   
#define SPK_WS            1   
#define I2S_DOUT          10  

static int16_t *audioBuffer = NULL;  
static Audio audio;
static bool isSpeaking = false;

static String lastTranscript = "";
static bool reviewMode = false;
static bool lastButtonState = HIGH;

const char* groqApiKey = "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";
const char* groqHost = "api.groq.com";
const char* geminiHost = "generativelanguage.googleapis.com";
const char* whisperModel = "whisper-large-v3-turbo";
const char* geminiModel = "gemini-2.5-flash";

const char* geminiKeys[] = {
  "AQ.Ab8RN6ITbxdJgNbr6XWzdjjDLd2pQ1LuBRJ153zHirrBjU6UUQ",
  "AQ.Ab8RN6K1Z31ChigIFZkcavXBjtnlhd1QBo0nEFhHgYr8Nc1KgQ",
  "AQ.Ab8RN6JcCC_uDzu8dwR2kB260zuwSSJwjqAJJGbBPmkresv_Yw",
  "AQ.Ab8RN6Id5atpU36LyTxAYkwgVWVD-DCA_0kS0770WRZ9xhf-Cw",
  "AQ.Ab8RN6LVjSHWcJbtcj1b_4hWj5w_ZbJ6vggxIkI-Xzz6kg5RPA",
  "AIzaSyCCdVA9kcIq2q-LpKUFpkCIA5L18PZy6Sk"
};
const int NUM_GEMINI_KEYS = sizeof(geminiKeys) / sizeof(geminiKeys[0]);
static int currentKeyIndex = 0;

static i2s_chan_handle_t rx_handle = NULL;

void setupAudioAndMic() {  
  audioBuffer = (int16_t*)ps_malloc(MAX_RECORD_SAMPLES * sizeof(int16_t));
  if (!audioBuffer) {
    Serial.println("PSRAM allocation failed");  
    updateDisplay("FATAL", "PSRAM Allocation Failed.");
    while (1);
  }  
  
  audio.setPinout(SPK_SCK, SPK_WS, I2S_DOUT);
  
  // Extract custom volume variable safely from preferences partition
  prefs.begin("system", true);
  int systemVolume = prefs.getInt("volume", 21);
  prefs.end();
  audio.setVolume(systemVolume);

  if (rx_handle != NULL) return;   
  
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);
  
  i2s_std_config_t std_cfg = {  
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {  
      .mclk = I2S_GPIO_UNUSED, .bclk = (gpio_num_t)MIC_SCK, .ws = (gpio_num_t)MIC_WS,
      .dout = I2S_GPIO_UNUSED, .din  = (gpio_num_t)I2S_SD,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
    }
  };
  
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  i2s_channel_init_std_mode(rx_handle, &std_cfg);
  i2s_channel_enable(rx_handle);
  Serial.println("Microphone driver active.");  
}  
  
void writeWavHeader(uint8_t *header, uint32_t dataSize) {  
  uint32_t fileSize = dataSize + 36;
  memcpy(header, "RIFF", 4);
  header[4] = fileSize & 0xFF; header[5] = (fileSize >> 8) & 0xFF; header[6] = (fileSize >> 16) & 0xFF; header[7] = (fileSize >> 24) & 0xFF;
  memcpy(header + 8, "WAVE", 4); memcpy(header + 12, "fmt ", 4);
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
  header[20] = 1;  header[21] = 0; header[22] = 1; header[23] = 0;
  uint32_t sampleRate = SAMPLE_RATE;
  header[24] = sampleRate & 0xFF; header[25] = (sampleRate >> 8) & 0xFF; header[26] = (sampleRate >> 16) & 0xFF; header[27] = (sampleRate >> 24) & 0xFF;
  uint32_t byteRate = SAMPLE_RATE * 2;
  header[28] = byteRate & 0xFF; header[29] = (byteRate >> 8) & 0xFF; header[30] = (byteRate >> 16) & 0xFF; header[31] = (byteRate >> 24) & 0xFF;
  header[32] = 2;  header[33] = 0; header[34] = 16; header[35] = 0;
  memcpy(header + 36, "data", 4);
  header[40] = dataSize & 0xFF; header[41] = (dataSize >> 8) & 0xFF; header[42] = (dataSize >> 16) & 0xFF; header[43] = (dataSize >> 24) & 0xFF;
}  
  
void endSpeechPlayback() {  
  audio.stopSong();
  delay(50);
  isSpeaking = false;
  setRGB(false, false, false); 
  if (rx_handle != NULL) i2s_channel_enable(rx_handle);
  updateDisplay("READY (AI APP)", "Hold Button (G3)\nto Record...\n\nClick G2 to see\nother App Pages ->");
}  
  
void processSpeechPlayback(String textToSpeak) {  
  Serial.println("\n===== GEMINI AI REPLY =====");  
  Serial.println(textToSpeak);
  updateDisplay("GEMINI AI", textToSpeak);
  if (rx_handle != NULL) i2s_channel_disable(rx_handle);
  audio.stopSong();
  delay(50);
  
  setRGB(false, true, false); 
  isSpeaking = true;
  audio.connecttospeech(textToSpeak.c_str(), "en");
}  
  
void askGeminiChatbot(String promptText) {  
  int attempts = 0;
  bool success = false;

  while (attempts < NUM_GEMINI_KEYS && !success) {
    setRGB(true, false, true); 
    updateDisplay("THINKING...", "Connecting (Key " + String(currentKeyIndex + 1) + ")...");
    WiFiClientSecure client;  
    client.setInsecure();  
    client.setTimeout(10);   
    
    if (!client.connect(geminiHost, 443)) {  
      currentKeyIndex = (currentKeyIndex + 1) % NUM_GEMINI_KEYS;
      attempts++;
      continue;  
    }  
    
    DynamicJsonDocument requestDoc(1024);  
    JsonObject systemInstruction = requestDoc.createNestedObject("systemInstruction");  
    JsonArray partsSystem = systemInstruction.createNestedArray("parts");  
    JsonObject partSysObj = partsSystem.createNestedObject();  
    partSysObj["text"] = "You are a concise Web3 voice assistant. Keep final responses under 15 words total. State numerical token prices explicitly.";
    
    JsonArray contents = requestDoc.createNestedArray("contents");
    JsonObject contentObj = contents.createNestedObject();
    contentObj["role"] = "user";
    JsonArray partsUser = contentObj.createNestedArray("parts");
    JsonObject partUserObj = partsUser.createNestedObject();
    partUserObj["text"] = promptText;
    
    JsonArray tools = requestDoc.createNestedArray("tools");
    JsonObject toolObj = tools.createNestedObject();
    toolObj.createNestedObject("googleSearch");
    
    String requestBody;  
    serializeJson(requestDoc, requestBody);
    
    String url = "/v1beta/models/" + String(geminiModel) + ":generateContent?key=" + String(geminiKeys[currentKeyIndex]);  
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + String(geminiHost));
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(requestBody.length());
    client.println("Connection: close");   
    client.println();  
    client.print(requestBody);  
    
    String statusLine = client.readStringUntil('\n');
    if (statusLine.indexOf("429") >= 0 || statusLine.indexOf("403") >= 0 || statusLine.indexOf("400") >= 0) {
      client.stop();
      currentKeyIndex = (currentKeyIndex + 1) % NUM_GEMINI_KEYS;
      attempts++;
      delay(200);
      continue;
    }

    while (client.connected()) {  
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }  
    
    String response = "";  
    while (client.available() || client.connected()) {
      if (client.available()) {
        char c = client.read();
        response += c;
      }  
    }  
    client.stop();  
    
    int jsonPos = response.indexOf('{');
    if (jsonPos >= 0) {  
      String json = response.substring(jsonPos);
      DynamicJsonDocument replyDoc(4096);   
      DeserializationError err = deserializeJson(replyDoc, json);
    
      if (!err) {  
        JsonVariant textPart = replyDoc["candidates"][0]["content"]["parts"][0]["text"];  
        if (textPart.is<String>() && textPart.as<String>().length() > 0) {  
          String aiReply = textPart.as<String>();  
          aiReply.replace("\n", " ");   
          aiReply.trim();  
          processSpeechPlayback(aiReply);  
          success = true;
          return;  
        }  
      }  
    }  
    currentKeyIndex = (currentKeyIndex + 1) % NUM_GEMINI_KEYS;
    attempts++;
  }  
  if (!success) {
    setRGB(true, false, false); 
    updateDisplay("ERROR", "All Gemini keys\nare exhausted!");
    delay(2000);
    setRGB(false, false, false);
  }
}  
  
String uploadToGroq(uint8_t *wavBuffer, uint32_t wavSize) {  
  setRGB(true, false, true); 
  updateDisplay("PROCESSING...", "Transcribing audio...");
  WiFiClientSecure client;  
  client.setInsecure();  
  
  if (!client.connect(groqHost, 443)) return "";
  
  String boundary = "----ESP32Boundary123456";
  String part1 = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n" + String(whisperModel) + "\r\n";
  String part2 = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String ending = "\r\n--" + boundary + "--\r\n";
  uint32_t contentLength = part1.length() + part2.length() + wavSize + ending.length();
  
  client.println("POST /openai/v1/audio/transcriptions HTTP/1.1");
  client.println("Host: " + String(groqHost));
  client.println("Authorization: Bearer " + String(groqApiKey));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.print("Content-Length: ");
  client.println(contentLength);
  client.println();
  client.print(part1);
  client.print(part2);
  client.write(wavBuffer, wavSize);
  client.print(ending);
  
  while (client.connected()) {  
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }  
  
  String response = "";  
  while (client.available()) response += client.readString();
  client.stop();  
  
  int jsonPos = response.indexOf('{');
  if (jsonPos >= 0) {  
    String json = response.substring(jsonPos);
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, json);
    if (!err && doc["text"]) return doc["text"].as<String>();
  }  
  return "";  
}  
  
void handleAIAssistant() {  
  if (isSpeaking) {  
    audio.loop();
    if (digitalRead(BUTTON_PIN) == LOW) {  
      Serial.println("\n[MANUAL OVERRIDE] Audio cut short by user request...");  
      endSpeechPlayback();  
      delay(300); 
    }  
    if (!audio.isRunning()) endSpeechPlayback();  
    return;   
  }  
  
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    delay(50);   
    if (digitalRead(BUTTON_PIN) == LOW) {  
        
      if (reviewMode) {  
        reviewMode = false;  
        askGeminiChatbot(lastTranscript);  
        lastButtonState = currentButtonState;  
        return;  
      }  
  
      setRGB(true, false, false); 
      updateDisplay("RECORDING...", "Release button\nwhen finished.");  
      
      int sampleIdx = 0;  
      while (digitalRead(BUTTON_PIN) == LOW && sampleIdx < MAX_RECORD_SAMPLES) {
        int32_t sample = 0;  
        size_t bytes_read = 0;  
        if (rx_handle != NULL) {  
          i2s_channel_read(rx_handle, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);
          audioBuffer[sampleIdx] = sample >> 14;   
          sampleIdx++;  
        }  
      }  
      setRGB(false, false, false); 
  
      uint32_t audioSize = sampleIdx * sizeof(int16_t);  
      uint32_t wavSize = audioSize + 44;  
  
      uint8_t *wavBuffer = (uint8_t*)ps_malloc(wavSize);  
      if (!wavBuffer) return;  
  
      writeWavHeader(wavBuffer, audioSize);  
      memcpy(wavBuffer + 44, audioBuffer, audioSize);  
  
      String textResult = uploadToGroq(wavBuffer, wavSize);  
      free(wavBuffer);  
  
      if (textResult != "") {  
        lastTranscript = textResult;  
        reviewMode = true;  
        setRGB(false, false, true); 
        String reviewInstructions = lastTranscript + "\n\n-> Click once: SEND\n-> Hold again: RE-DO";  
        updateDisplay("CONFIRM TEXT", reviewInstructions);  
      } else {  
        setRGB(true, false, false);
        updateDisplay("ERROR", "Could not hear audio.\nTry holding again.");  
        delay(1500);
        setRGB(false, false, false);
        reviewMode = false;  
      }  
      delay(200);   
    }  
  }  
  lastButtonState = currentButtonState;
}  

void audio_info(const char* info) {}  
void audio_eof_speech(const char* info) { endSpeechPlayback(); }  
#endif
