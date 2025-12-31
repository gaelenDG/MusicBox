// helpers.h - function prototypes for helpers.cpp

#ifndef HELPERS_H
#define HELPERS_H

// ======== Library initialization ========
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_PN532.h>
#include "tag_parser.h"

// ======== Function prototypes ======== //

// All defined in helpers.cpp

int readRFID();
int readVolumeKnob();
String readNdefTextFromTag();
TagCommand parseTagPayload(const String& payload);
bool readTagPage(uint8_t page, uint8_t *buf);
float readBatVoltage();
void checkBatteryAndSleepIfLow();
bool checkDFPlayerConnection();
void checkPeripherals();
void setStatusLight(uint8_t RedVal, uint8_t GreenVal, uint8_t BlueVal);


#pragma once

enum ButtonAction {
  BUTTON_NONE,
  BUTTON_SHORT,
  BUTTON_LONG,
  BUTTON_REPEAT
};

ButtonAction checkButton(uint8_t pin);
void handleButtonAction();

#endif // HELPERS_H