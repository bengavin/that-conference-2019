/*
 * Project particle-mind
 * Description: The main source file for the 'Particle Mind' client
 * Author: Ben Gavin <ben@virtual-olympus.com>
 * Date: 2019-06-01
 */

#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);

/*** Neopixel LED Interactions ***/
#include "neopixel.h"

// IMPORTANT: Set pixel COUNT, PIN and TYPE
#define PIXEL_PIN D2
#define PIXEL_COUNT 5
#define PIXEL_TYPE WS2812B

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

void rainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);

/*** /Neopixel LED Interactions ***/

/*** Click Button Support ***/
#include "clickButton.h"
#define RED_BUTTON_INPUT_PIN D9
#define RED_BUTTON_INDICATOR_PIN D4
#define GREEN_BUTTON_INPUT_PIN D11
#define GREEN_BUTTON_INDICATOR_PIN D6
 
// We want a button that starts as 'inactive' and uses an internal
// 'pullup' resistor.  This means that when the button isn't pressed
// the input will read as 'HIGH' (3.3v) and we'll connect the button
// to ground, such that pressing the button will cause electricity
// flow to ground and bring the reading to 'LOW' (0v)
ClickButton redButton(RED_BUTTON_INPUT_PIN, false, true);
ClickButton greenButton(GREEN_BUTTON_INPUT_PIN, false, true);

void setButtonIndicator(ClickButton button, uint16_t indicatorPin);

/*** /Click Button Support ***/

// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.
  // Indicate that we're in setup
  pinMode(D7, OUTPUT);
  digitalWrite(D7, HIGH);

  // Initialize our NeoPixel strip
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Setup our cycle button (not needed with our 'game' logic)
  //// Setup button timers (all in milliseconds / ms)
  //cycleButton.debounceTime   = 20;   // Debounce timer in ms
  //cycleButton.multiclickTime = 750;  // Time limit for multi clicks (all clicks must occur within this timeout)
  //cycleButton.longClickTime  = 1000; // time until "held-down clicks" register

  // Button indicator LED
  pinMode(RED_BUTTON_INDICATOR_PIN, OUTPUT);
  digitalWrite(RED_BUTTON_INDICATOR_PIN, LOW);
  pinMode(GREEN_BUTTON_INDICATOR_PIN, OUTPUT);
  digitalWrite(GREEN_BUTTON_INDICATOR_PIN, LOW);

  // Indicate Setup is complete (with light pattern)
  for(int i = 0; i < 3; i++)
  {
    digitalWrite(D7, LOW);
    delay(250);
    digitalWrite(D7, HIGH);
  }
  digitalWrite(D7, LOW);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core of your code will likely live here.

  // Check the buttons
  setButtonIndicator(redButton, RED_BUTTON_INDICATOR_PIN);
  setButtonIndicator(greenButton, GREEN_BUTTON_INDICATOR_PIN);

  // Step the rainbow
  rainbow(50);
}

void setButtonIndicator(ClickButton button, uint16_t indicatorPin) {
  // Check button state
  button.Update();

  // Set indicator light
  if (button.depressed)
  {
      digitalWrite(indicatorPin, HIGH);
  }
  else 
  {
      digitalWrite(indicatorPin, LOW);
  }
}

int rainbow_step = 0;
void rainbow(uint8_t wait) {
  // Not entirely button/loop friendly, but much closer, can pull in 
  // ellapsedMillis library if desired later to get better performance
  uint16_t i;

  if (rainbow_step >= 256) { rainbow_step = 0; }

  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel((i+rainbow_step) & 255));
  }
  strip.show();

  rainbow_step++;

  delay(wait);
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
