#ifndef PERIPHERALS_H
#define PERIPHERALS_H

// Forward declarations only - include concrete headers in .cpp files that need them
class Adafruit_NeoPixel;
class DFRobotDFPlayerMini;
class HardwareSerial;
class Adafruit_PN532;

// Peripherals defined in src/config.cpp
extern Adafruit_NeoPixel StatusLight;
extern DFRobotDFPlayerMini player;
extern HardwareSerial MP3Serial; // using HardwareSerial for ESP32 hardware UART
extern Adafruit_PN532 nfc;

#endif // PERIPHERALS_H
