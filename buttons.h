#pragma once

#include <AceButton.h>
using namespace ace_button;

ButtonConfig modeButtonConfig;
AceButton modeButton(&modeButtonConfig);

uint8_t modeNumber = 0;

#define NUM_MODES 4

void modeButtonEventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
  switch (eventType)
  {
    case AceButton::kEventPressed:
      break;
    case AceButton::kEventReleased:
      modeNumber = (modeNumber + 1) % NUM_MODES;
      break;
  }
}

void setupButtons()
{
  pinMode(0, INPUT_PULLUP);
  modeButton.init((uint8_t)0);
  modeButtonConfig.setEventHandler(modeButtonEventHandler);
}

void checkButtons()
{
  static uint16_t prev = millis();

  uint16_t now = millis();
  if ((uint16_t)(now - prev) >= 5)
  {
    modeButton.check();
    prev = now;
  }
}
