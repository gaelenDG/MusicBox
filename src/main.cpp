// ======== Library initialization ========
#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include <Adafruit_NeoPixel.h>
#include "esp32-hal-gpio.h"
#include "peripherals.h"
#include "config.h"
#include "helpers.h"
#include "tag_parser.h"

// using namespace std;


void setup() {

  // Init USB serial port for debugging
  Serial.begin(9600);

  // Initialize status light
  pinMode(StatusLight_R_Pin, OUTPUT);
  pinMode(StatusLight_G_Pin, OUTPUT);
  pinMode(StatusLight_B_Pin, OUTPUT);

  // Show booting up with dim yellow
  setStatusLight(10, 5, 0);
  

  // Initialize buttons
  pinMode(Button1_pin, INPUT);
  pinMode(Button2_pin, INPUT);

  // initialize power switch
  pinMode(Switch_pin, INPUT_PULLUP);

  // If switch is OFF at startup, immediately sleep
  if (digitalRead(Switch_pin) == HIGH) {  // <-- now HIGH means OFF
    Serial.println("🔌 Switch is OFF — entering deep sleep.");
    esp_sleep_enable_ext1_wakeup(1ULL << Switch_pin, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
  }

  // Do a check of battery status on bootup
  checkBatteryAndSleepIfLow();

  // ----------- Initialize MP3 UART connection ------------------
  bool DFRobot_connected = false; // little check for the MP3 player connection

  MP3Serial.begin(9600, SERIAL_8N1, /*rx*/ 17, /*tx*/ 16); 
  delay(250);
  unsigned long t0 = millis();
  while (millis() - t0 < 2000) {
    if (MP3Serial.available()) Serial.printf("Got: 0x%02X\n", MP3Serial.read());
  }


  if (!player.begin(MP3Serial)) {
    Serial.println("❌ Connecting to DFPlayer Mini failed!");
    Serial.println("Check wiring, SD card, and voltage levels.");
    setStatusLight(0, 10, 10); 

    DFRobot_connected = false; 

  } 

  DFRobot_connected = true; 

  Serial.println("DFRobot player connected!");

  
  

  // ----------- Initialize NFC reader ------------------
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  bool NFCconnected = false;
  if (!versiondata) {
    Serial.println("Didn't find PN53x board");

    setStatusLight(10, 0, 10);

  } else {
    NFCconnected = true;
    Serial.println("PN532 connected!");
    nfc.SAMConfig();
  }

  
  // --------- Final check, update status light if ready--------- 
  checkPeripherals();
  if (DFRobot_connected && NFCconnected) {
    setStatusLight(0, 5, 0);  // green = all good

    // Play removed chime then stop playback
    player.volume(10);
    player.playFolder(01,001);

    // Instead of hard/blocking delay, soft yield
    unsigned long chimeStart = millis();
    while (millis() - chimeStart < 2000) {
      yield(); // feed OS/wdt so we won't panic
    }

    player.volume(volume);
    player.stop();
  }
  
}

void loop() {

  pinMode(Switch_pin, INPUT_PULLUP);
  bool switchOn = (digitalRead(Switch_pin) == LOW);


  // Check battery levels, put to sleep if not charged sufficiently
  checkBatteryAndSleepIfLow();

  // Check for button presses, perform actions
  handleButtonAction();

  // Wait for a card
  boolean success;
  uint8_t uid[7];
  uint8_t uidLen = 0;
  static unsigned long lastCooldown = 0;

  // 
  if (millis() - lastNfcCheck >= nfcInterval) {

    checkPeripherals();
    
    lastNfcCheck = millis();

    Serial.println("Waiting for a tag... (tap now)");

    bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 50);

    // If tag detected, build new string for comparison with global
    if (success) {

      // Change status light to show playing
      setStatusLight(0, 0, 5); 

      // update last seen time
      lastTagSeen = millis();

      // If new tag detected 

      bool newTag = !tagPresent || (uidLen != currentUIDLength) || (memcmp(uid, currentUID, uidLen) != 0);

      if (newTag) {

        player.volume(volume);
  
        tagPresent = true;
        memcpy(currentUID, uid, uidLen);
        currentUIDLength = uidLen;
        hasPlayedForCurrentTag = false;  // allow playback for this tag

        // Print UID as hex
        Serial.print("Current UID: ");
        for (uint8_t i = 0; i < currentUIDLength; i++) {
          if (currentUID[i] < 0x10) Serial.print("0"); // leading zero
          Serial.print(currentUID[i], HEX);
        }
        Serial.println();

      } 

      // Here could be where I map the records to their respective files...
      // For now, just pulling the folder from the payload and playing.
      // Attempt to read NDEF text payload
      // Triggers playback only once per tag reading
      if (!hasPlayedForCurrentTag) {
        String payload = readNdefTextFromTag();
        if (payload.length() == 0) {
          Serial.println("No NDEF text found (or read failed). Ensure tag is NDEF formatted and contains a Text record.");
          // Change status light to show removed 
          setStatusLight(10, 0, 0); 

        } else {
          Serial.print("NDEF text payload: '");
          Serial.print(payload);
          Serial.println("'");

          // Parse the tag payload for folder, track, volume, shuffle
          TagCommand cmd = parseTagPayload(payload);

          if (cmd.valid) {
            Serial.printf("Parsed tag: folder=%u, track=%u, volume=%d, shuffle=%s\n",
                          cmd.folder, cmd.track, cmd.volume, cmd.shuffle ? "yes" : "no");

            // Set volume if specified
            if (cmd.volume >= 0) {
              volume_boost = cmd.volume;
              Serial.printf("Changing volume by %d to a total of %d\n", cmd.volume, min(volume + cmd.volume, 30));
              player.volume(min(volume + cmd.volume, 30));
              volume = volume + cmd.volume;
            }

            // Enable shuffle if specified
            if (cmd.shuffle) {
              Serial.println("Shuffle enabled");
              player.randomAll();
            }

            // Play the track
            Serial.printf("Playing folder %u.\n", cmd.folder);
            player.loopFolder(cmd.folder);
            hasPlayedForCurrentTag = true;
          } else {
            Serial.println("Failed to parse tag payload.");
            setStatusLight(10, 0, 0);
          }
        }
      }

    } else {

      // No tag read this iteration: check timeout to decide removal
      if (tagPresent && millis() - lastTagSeen > TAG_TIMEOUT) {
        tagPresent = false;
        hasPlayedForCurrentTag = false;  // reset so next tag triggers playback
        Serial.println("Tag removed - stopping playback");

        // Change status light to show removed 
        setStatusLight(0, 5, 0); 

        // Play removed chime at fixed volume (always 10)
        player.volume(10);
        player.playFolder(01, 002);
        
        // Instead of hard/blocking delay, soft yield
        unsigned long chimeStart = millis();
        while (millis() - chimeStart < 2500) {
          yield(); // feed OS/wdt so we won't panic
        }

        // Restore the volume setting (from tag or default)
        volume = volume - volume_boost; // remove volume boost
        player.volume(volume);
        player.stop();


        // Clear UID state
        currentUIDLength = 0;
        memset(currentUID, 0, sizeof(currentUID));
      }
    }
    
  }

  // Short delay to prevent busy-looping
  delay(10);
}
