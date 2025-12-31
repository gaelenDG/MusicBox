// ======== Library initialization ========
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_PN532.h>
#include <Wire.h>
#include <SPI.h>
#include "helpers.h"
#include "HardwareSerial.h"
#include "esp_sleep.h"
#include "peripherals.h"
#include "config.h"
#include <DFRobotDFPlayerMini.h>
using namespace std;


// ======== Function definitions ========

// Set status light color
void setStatusLight(uint8_t RedVal, uint8_t GreenVal, uint8_t BlueVal) {
  analogWrite(StatusLight_R_Pin,   RedVal);
  analogWrite(StatusLight_G_Pin, GreenVal);
  analogWrite(StatusLight_B_Pin,  BlueVal);
}


// Read raw NTAG page (4..n). Each page = 4 bytes
bool readTagPage(uint8_t page, uint8_t *buf) {
  // ntag2xx_ReadPage returns true on success (fills 4 bytes)
  return nfc.ntag2xx_ReadPage(page, buf);
}

// Extract NDEF payload (text record) from tag. Returns empty string if none/failure.
String readNdefTextFromTag() {
  // Buffer to hold a reasonable chunk of user memory. Most NTAGs give pages 4..39
  const uint8_t startPage = 4;
  const uint8_t maxPages = 16; // read up to 16 pages => 16*4 = 64 bytes (adjust if you need more)
  uint8_t raw[4 * maxPages];
  memset(raw, 0, sizeof(raw));

  // Read pages into raw[]
  for (uint8_t i = 0; i < maxPages; ++i) {
    uint8_t pageBuf[4];
    if (!readTagPage(startPage + i, pageBuf)) {
      // read failed; tag might not be NTAG or out-of-range
      Serial.printf("Failed to read page %u\n", startPage + i);
      return String("");
    }
    memcpy(raw + i * 4, pageBuf, 4);
  }

  // Search TLV for 0x03 (NDEF message TLV)
  // TLV structure: [TAG][LEN][VALUE...], TAG=0x03 for NDEF, 0x00 = NULL, 0xFE = terminator
  int idx = 0;
  int maxBytes = 4 * maxPages;
  while (idx < maxBytes) {
    uint8_t tag = raw[idx];
    if (tag == 0x00) { idx++; continue; }      // NULL TLV -> skip
    if (tag == 0xFE) { break; }               // terminator -> stop
    if (tag != 0x03) {
      // Unknown TLV tag: skip it using length field
      if (idx + 1 >= maxBytes) break;
      uint8_t len = raw[idx + 1];
      idx += 2 + len;
      continue;
    }

    // Found NDEF TLV
    if (idx + 1 >= maxBytes) break;
    uint8_t len = raw[idx + 1];
    int ndefStart = idx + 2;
    if (len == 0xFF) {
      // extended length not handled in this short example
      Serial.println("Extended length NDEF not supported in this example.");
      return String("");
    }
    if (ndefStart + len > maxBytes) {
      Serial.println("Not enough bytes read for full NDEF, increase maxPages.");
      return String("");
}

    // Now parse the NDEF message. We expect a single NDEF record (Text record typical):
    // NDEF record header: [TNF/MB/ME/CF/SR/IL/TYPE_LEN] [PAYLOAD_LEN] [TYPE] [PAYLOAD...]
    // Common short-record text: 0xD1 0x01 <payloadLen> 0x54 <status> <lang> <text...>
    uint8_t *ndef = raw + ndefStart;
    uint8_t hdr = ndef[0];
    bool sr = hdr & 0x10; // short record flag
    uint8_t typeLen = ndef[1];
    uint32_t payloadLen = 0;
    int offset = 2;

    if (sr) {
      payloadLen = ndef[offset++];
    } else {
      // 4-byte payload length (not expected for small text records)
      payloadLen = (ndef[offset] << 24) | (ndef[offset+1] << 16) | (ndef[offset+2] << 8) | ndef[offset+3];
      offset += 4;
    }

    uint8_t idLen = 0;
    // If IL flag set (0x08) then next byte is id length
    if (hdr & 0x08) {
      idLen = ndef[offset++];
    }

    // Type field (typeLen bytes)
    char typeBuf[32] = {0};
    if (typeLen >= (int)sizeof(typeBuf)) typeLen = sizeof(typeBuf)-1;
    memcpy(typeBuf, ndef + offset, typeLen);
    offset += typeLen;

    // Skip ID if present
    if (idLen) offset += idLen;

    // Payload starts at ndef + offset; payloadLen bytes
    if (payloadLen == 0) return String("");

    uint8_t *payload = ndef + offset;

    // If this is a Text record type ("T" -> 0x54), the payload structure:
    // [status byte][language code][text...]
    if (typeLen == 1 && typeBuf[0] == 'T') {
      uint8_t status = payload[0];
      uint8_t langLen = status & 0x3F;
      int textIndex = 1 + langLen;
      if ((int)payloadLen <= textIndex) {
        return String("");
      }
      
      // Extract text
      int textLen = payloadLen - textIndex;
      char textBuf[256] = {0};
      if (textLen >= (int)sizeof(textBuf)) textLen = sizeof(textBuf) - 1;
      memcpy(textBuf, payload + textIndex, textLen);
      
      return String(textBuf);
    } else {
      // Not a text record: for debugging, return raw bytes as hex or attempt to print
      String s = "";
      for (uint32_t i = 0; i < payloadLen; ++i) {
        char c = payload[i];
        // if printable, append, otherwise show hex
        if (c >= 32 && c <= 126) s += c;
        else {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\x%02X", (uint8_t)c);
          s += buf;
        }
      }
      return s;
    }
  }

  // No NDEF TLV found
  return String("");
}


TagCommand parseTagPayload(const String& payload) {
  TagCommand cmd = {0, 1, -1, false, false};  // default track to 1
  
  Serial.printf("Parsing payload (len=%d): '%s'\n", payload.length(), payload.c_str());
  
  if (payload.length() == 0) {
    return cmd;
  }

  // Find the pipe delimiter to split folder from options
  int pipeIndex = payload.indexOf('|');
  String folderStr;
  String optionsStr;

  if (pipeIndex > 0) {
    folderStr = payload.substring(0, pipeIndex);
    optionsStr = payload.substring(pipeIndex + 1);
  } else {
    folderStr = payload;
    optionsStr = "";
  }

  // Parse folder (e.g., "07")
  folderStr.trim();
  cmd.folder = folderStr.toInt();

  if (cmd.folder == 0) {
    // Invalid folder (0 is not valid in DFPlayer)
    Serial.printf("Invalid folder: %d\n", cmd.folder);
    return cmd;
  }

  Serial.printf("Parsed folder: %u, track: %u\n", cmd.folder, cmd.track);

  // Parse options (e.g., "volume 5 | shuffle")
  optionsStr.toLowerCase(); // case-insensitive
  
  // Look for "volume" keyword
  int volIndex = optionsStr.indexOf("volume");
  if (volIndex >= 0) {
    // Extract the number after "volume"
    int numStart = volIndex + 6; // length of "volume"
    while (numStart < optionsStr.length() && !isdigit(optionsStr[numStart])) {
      numStart++;
    }
    if (numStart < optionsStr.length()) {
      int numEnd = numStart;
      while (numEnd < optionsStr.length() && isdigit(optionsStr[numEnd])) {
        numEnd++;
      }
      String volStr = optionsStr.substring(numStart, numEnd);
      int vol = volStr.toInt();
      if (vol >= 0 && vol <= 30) {
        cmd.volume = vol;
        Serial.printf("Parsed volume: %d\n", cmd.volume);
      }
    }
  }

  // Check for "shuffle" keyword
  if (optionsStr.indexOf("shuffle") >= 0) {
    cmd.shuffle = true;
    Serial.println("Shuffle enabled");
  }

  cmd.valid = true;
  return cmd;
}


float readBatVoltage() {
  uint32_t Vbatt = 0;
  for(int i = 0; i < 16; i++) {
    Vbatt += analogReadMilliVolts(4); // Read and accumulate ADC voltage
    delay(2);
  }
  float Vbattf = 2 * Vbatt / 16 / 1000.0; 

  return Vbattf;
}

void checkBatteryAndSleepIfLow() {
  unsigned long Batt_now = millis();

  // Skip battery check if it hasn't been very long since last check
  if (Batt_now - lastBattCheck < Batt_Check_Interval) return;

  // Update last check timestamp immediately
  lastBattCheck = Batt_now;

  float Vbat = readBatVoltage();
  Serial.printf("Battery voltage: %.2f V\n", Vbat);

  // Case 1: No battery connected
  if (Vbat < NO_BAT_THRESHOLD) {
    Serial.println("⚠️ No battery detected — continuing normal operation.");
    return;
  }

  // Case 2: Battery low
  if (Vbat < LOW_BAT_THRESHOLD) {
    Serial.println("Low battery! Charge me!");

    // Play low-battery chime
    player.volume(10);
    player.playFolder(1, 3); // folder 01, file 001

    unsigned long chimeStart = millis();
    while (millis() - chimeStart < 2500) {
      yield(); // keep watchdog fed during wait
    }

    player.stop();
    player.volume(volume);

    // Compute sleep duration in microseconds
    uint64_t sleep_us = (uint64_t)LOW_BAT_SLEEP_INTERVAL * 120ULL * 1000000ULL;

    Serial.printf("Sleeping for %d minutes before rechecking battery.\n",
                  LOW_BAT_SLEEP_INTERVAL);

    // Configure wake timer
    esp_sleep_enable_timer_wakeup(sleep_us);
    esp_deep_sleep_start();
  }

  // Case 3: Battery OK — continue running
  Serial.println("Battery OK — continuing normal operation.");
}
// --- DFPlayer activity tracking ---
unsigned long lastDFPlayerActivity = 0;
const unsigned long DFPLAYER_SILENCE_TIMEOUT = 30000; // 30s timeout

bool checkDFPlayerConnection() {
  // Try to read volume — safe "ping"
  int vol = player.readVolume();

  // ✅ Successful response
  if (vol >= 0 && vol <= 30) {
    lastDFPlayerActivity = millis();
    return true;
  }

  // ⚠️ No valid response — see how long it’s been silent
  if (millis() - lastDFPlayerActivity > DFPLAYER_SILENCE_TIMEOUT) {
    Serial.println("⚠️ DFPlayer unresponsive for too long — forcing reconnect...");
    return false;
  }

  // Might just be in a brief busy state — don’t panic yet
  return true;
}

void checkPeripherals() {
  static unsigned long lastPeripheralCheck = 0;
  static bool dfPreviouslyOK = true;
  static bool nfcPreviouslyOK = true;
  static int dfFails = 0;
  static int nfcFails = 0;

  unsigned long now = millis();
  if (now - lastPeripheralCheck < CHECK_INTERVAL) return;
  lastPeripheralCheck = now;

  Serial.println("🔍 Checking peripherals...");

  // ---- DFPlayer health check ----
  bool DFPlayer_connected = checkDFPlayerConnection();

  if (DFPlayer_connected) {
    dfplayerOK = true;
    dfFails = 0;
  } else if (++dfFails >= 2) { // Allow two consecutive fails before reconnect
    dfplayerOK = false;
    dfFails = 0;
    Serial.println("❌ DFPlayer unresponsive — attempting reconnection...");
    MP3Serial.begin(9600, SERIAL_8N1, 17, 16);
    delay(200);
    player.begin(MP3Serial);
    player.volume(20);
    setStatusLight(10, 0, 10);
    lastDFPlayerActivity = millis(); // reset timer after reconnect
  }

  // ---- PN532 check (unchanged) ----
  bool nfcNowOK = false;
  uint32_t versiondata = nfc.getFirmwareVersion();

  if (versiondata) {
    nfcNowOK = true;
    nfcFails = 0;
  } else {
    Serial.println("⚠️ PN532 not responding...");
    if (++nfcFails >= 2) {
      nfcFails = 0;
      Serial.println("❌ Attempting PN532 reconnection...");
      nfc.begin();
      delay(100);
      versiondata = nfc.getFirmwareVersion();
      if (versiondata) {
        nfcNowOK = true;
        nfc.SAMConfig();
        Serial.println("✅ PN532 reconnected!");
      } else {
        Serial.println("🚫 PN532 still disconnected.");
        nfcNowOK = false;
      }
    }
  }

  nfcOK = nfcNowOK;

  // ---- Status light ----
  if (dfplayerOK && nfcOK) {
    setStatusLight(0, 5, 0);  // green = all good

     Serial.println("Peripherals good!");

  } else if (!dfplayerOK && !nfcOK) {
    setStatusLight(10, 0, 0); // red = both failed
  } else if (!dfplayerOK) {
    setStatusLight(0, 5, 5);  // cyan = DFPlayer issue
  } else if (!nfcOK) {
    setStatusLight(5, 0, 5);  // magenta = NFC issue
  }

  // ---- Log transitions ----
  if (dfplayerOK != dfPreviouslyOK)
    Serial.printf("🎵 DFPlayer state changed → %s\n", dfplayerOK ? "Connected" : "Disconnected");
  if (nfcOK != nfcPreviouslyOK)
    Serial.printf("📶 PN532 state changed → %s\n", nfcOK ? "Connected" : "Disconnected");


  dfPreviouslyOK = dfplayerOK;
  nfcPreviouslyOK = nfcOK;
}


// ---- Observe button presses (currently only 2 buttons) ----
ButtonAction checkButton(uint8_t pin) {
  static unsigned long pressStart[2] = {0, 0};
  static bool buttonPressed[2] = {false, false};
  static bool longPressFired[2] = {false, false};

  int index = (pin == Button1_pin) ? 0 : 1;
  ButtonAction action = BUTTON_NONE;
  bool isPressed = (digitalRead(pin) == LOW);
  unsigned long now = millis();

  // --- Button just pressed ---
  if (isPressed && !buttonPressed[index]) {
    buttonPressed[index] = true;
    pressStart[index] = now;
    longPressFired[index] = false;
  }

  // --- Button is being held ---
  if (buttonPressed[index] && isPressed) {
    if (!longPressFired[index] && (now - pressStart[index] > LONG_PRESS_TIME)) {
      longPressFired[index] = true;
      action = BUTTON_LONG;
    }
  }

  // --- Button just released ---
  if (!isPressed && buttonPressed[index]) {
    buttonPressed[index] = false;
    if (!longPressFired[index] && (now - pressStart[index] > DEBOUNCE_TIME)) {
      action = BUTTON_SHORT;
    }
  }

  return action;
}

// ---- Trigger button actions, skip/previous tracks
void handleButtonAction_skip() {
  // Buttons trigger song skip forward/back

  ButtonAction upAction = checkButton(Button2_pin);
  ButtonAction downAction = checkButton(Button1_pin);


  if (upAction == BUTTON_SHORT || upAction == BUTTON_LONG) {

    player.next();
    Serial.println("Skipping to next track");

    
  }

  if (downAction == BUTTON_SHORT || downAction == BUTTON_LONG) {
    player.previous();
    Serial.println("Going back to previous track");
  }
}

void handleButtonAction() {
  // Volume control for buttons

  ButtonAction upAction = checkButton(Button2_pin);
  ButtonAction downAction = checkButton(Button1_pin);

  if (upAction == BUTTON_SHORT || upAction == BUTTON_LONG) {
    volume = min(MAX_VOLUME, volume + 1);
    player.volume(volume);
    Serial.printf("Volume up: %d\n", volume);
  }

  if (downAction == BUTTON_SHORT || downAction == BUTTON_LONG) {
    volume = max(MIN_VOLUME, volume - 2);
    player.volume(volume);
    Serial.printf("Volume down: %d\n", volume);
  }
}
