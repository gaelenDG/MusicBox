#ifndef CONFIG_H
#define CONFIG_H

// ======== Library initialization ========
// Keep includes minimal in headers to avoid include-depth problems.
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>


// Forward declarations for types defined in .cpp files / external libs
class Adafruit_PN532; // forward declare PN532 class
class DFRobotDFPlayerMini; // forward declare DFPlayer class


// ======== Configuration constants ========
// Keep config.h small; peripheral objects live in peripherals.h

// ======== GPIO & I2C Declarations ========
extern const int Button1_pin; // GPIO pin for capacitive touch button data line
extern const int Button2_pin; // GPIO pin for capacitive touch button data line
extern uint8_t VoltageReader_Pin; // GPIO pin for battery voltage reader
extern uint8_t StatusLight_R_Pin;
extern uint8_t StatusLight_G_Pin;
extern uint8_t StatusLight_B_Pin;
extern uint8_t Switch_pin; // Deep sleep switch

// ======== Global variables ========
extern unsigned long lastTagSeen;
extern bool hasPlayedForCurrentTag;
extern bool tagPresent;
extern uint8_t currentUID[7];
extern uint8_t currentUIDLength;
extern const int DEFAULT_VOLUME;
extern int volume;
extern int volume_boost; 
extern int MAX_VOLUME;
extern int MIN_VOLUME;
extern const unsigned long TAG_TIMEOUT; // ms: consider 700-1500 depending on your UX
extern const unsigned long POST_READ_COOLDOWN; // ms to wait after a successful read

// Button press timing
extern uint64_t LONG_PRESS_TIME;
extern uint64_t DEBOUNCE_TIME;
extern uint64_t REPEAT_INTERVAL;

// NFC reader timing
extern unsigned long lastNfcCheck;
extern const unsigned long nfcInterval;


// Battery checking timing and thresholds
extern float NO_BAT_THRESHOLD;
extern float LOW_BAT_THRESHOLD; // A limit point to trigger deep sleep if the battery is too low
extern uint64_t Batt_Check_Interval;
extern uint64_t LOW_BAT_SLEEP_INTERVAL; // Minutes between battery checks while charging
extern int WAKE_BAT_THRESHOLD; 
extern unsigned long lastBattCheck;

// Peripheral checking
extern bool dfplayerOK;
extern bool nfcOK;
extern unsigned long lastPeripheralCheck;
extern const unsigned long CHECK_INTERVAL;


#endif // CONFIG_H
