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
#define PIXEL_TYPE WS2812

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

/*** Timer Support ***/
#include "elapsedMillis.h"
/*** /Timer Support ***/

/*** Game Related Logic ***/
// High Level Functions (these are called continuously on each pass through the game loop)
void gatherUserInput();
void updateGameState();
void renderGameState();

#define COLOR_BLACK 0x00000000

// Our 'indicator' colors for 'yes color in set' (white) and 'yes color and position correct' (red)
#define COLOR_RED 0x00FF0000
#define COLOR_WHITE 0x00FFFFFF

// Possible color pin guesses, we'll cycle through these
#define COLOR_GREEN 0x0000AA00
#define COLOR_BLUE 0x000000AA
#define COLOR_PURPLE 0x00880088
#define COLOR_YELLOW 0x0088AA00
#define COLOR_ORANGE 0x00886600
#define COLOR_CYAN 0x0000CCCC

#define MAX_GUESS_ELEMENTS 5

// Fast inline macros for grabbing R/G/B pixel values
// ***NOTE: NeoPixel Particle library is broken, as it treats 
// ***NOTE: WS2812 as a GRB device, when it's RGB :(, so fix it here...
// TODO: This should be these...
//#define PXRED(c) ((uint8_t)((c >> 16) & 0xFF))
//#define PXGREEN(c) ((uint8_t)((c >> 8) & 0xFF))
// TODO: But needs to be these...
#define PXRED(c) ((uint8_t)((c >> 8) & 0xFF))
#define PXGREEN(c) ((uint8_t)((c >> 16) & 0xFF))
#define PXBLUE(c) ((uint8_t)(c & 0xFF))
#define PXDIM(c, pct) ((uint8_t)(c * pct))

// The list of colors we will cycle through
#define PEG_NONE -1
#define PEG_COLORS 6
const uint32_t pegColors[PEG_COLORS] = { COLOR_GREEN, COLOR_BLUE, COLOR_PURPLE, COLOR_YELLOW, COLOR_ORANGE, COLOR_CYAN };

// Information about our guesses so far
short currentGuessColors[MAX_GUESS_ELEMENTS] = { PEG_NONE, PEG_NONE, PEG_NONE, PEG_NONE, PEG_NONE };
int numberOfGuesses = 0;
elapsedMillis blinkTimer(0);
#define BLINK_CYCLE_TIME 3000

// Has the user clicked either of our 'action' buttons?
bool changeColorButtonClicked;
bool changeIndexButtonClicked;

// What is the current game element we're working with?
typedef enum _GameState { 
  STATE_GAME_START = 0,
  STATE_GUESS_START,
  STATE_GUESS,
  STATE_SUBMIT_GUESS,
  STATE_SHOW_GUESS_RESULT
} GameState;

// TODO: Replace this with something that asks for the game ready event
uint64_t gameStartTime = 5000; // Start the game after 5 seconds
elapsedMillis gameStartTimer(0);

GameState currentGameState = STATE_GAME_START;
int currentGuessIndex = 0;

/*** /Game Related Logic ***/

// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.
  // Indicate that we're in setup
  pinMode(D7, OUTPUT);
  digitalWrite(D7, HIGH);

  // Initialize our NeoPixel strip
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

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

  // Reset the game start timer
  gameStartTimer = 0;
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core 'game loop'
  gatherUserInput();

  updateGameState();

  renderGameState();

  // breath for 10ms
  delay(10);
}

void gatherUserInput() {
  // Get ready to check for clicks, this routine only ever
  // flags the 'click' as happening, the game update stage
  // will clear flags as needed depending on current game state
  redButton.Update();
  greenButton.Update();

  if (redButton.clicks > 0) {
    changeColorButtonClicked = true;
  }

  if (greenButton.clicks > 0) {
    changeIndexButtonClicked = true;
  }
}

void updateGameState() {
  // Here, we update game state depending on what we were doing
  // and the possible actions we might take during that phase
  switch (currentGameState) {
    case STATE_GAME_START:
      // Reset Current Guess Information
      for (int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
        currentGuessColors[i] = PEG_NONE;
      }
      numberOfGuesses = 0;

      // ignore any clicks (clear them out)
      changeColorButtonClicked = false;
      changeIndexButtonClicked = false;

      // TODO: Wait for the 'game start' mesh event
      if (gameStartTimer >= gameStartTime) {
        currentGameState = STATE_GUESS_START;
      }

      break;

    case STATE_GUESS_START:
        // We always start with the first item when guessing
        currentGuessIndex = 0;
        if (numberOfGuesses == 0) {
          // We've never guessed before, so let's assume they 
          // want to start with the first peg color
          currentGuessColors[currentGuessIndex] = 0;
        }

        // Now the user is guessing...
        blinkTimer = 0;
        currentGameState = STATE_GUESS;

        break;

    case STATE_GUESS:
        // Now things get hairy... what do we do here??
        if (changeColorButtonClicked) {
          changeColorButtonClicked = false;
          short currentGuess = currentGuessColors[currentGuessIndex];
          currentGuessColors[currentGuessIndex] = (currentGuess == PEG_NONE ? 0 : ++currentGuess) % PEG_COLORS;
        }
        else if (changeIndexButtonClicked) {
          changeIndexButtonClicked = false;
          currentGuessIndex = ++currentGuessIndex % MAX_GUESS_ELEMENTS;
        }

        break;
  }
}

float getDimLevel() {
  float cycleTime = blinkTimer % BLINK_CYCLE_TIME / (float)BLINK_CYCLE_TIME;
  if (cycleTime < 0.05 || cycleTime > 0.95) { return 0.0; }
  if (cycleTime < 0.10 || cycleTime > 0.90) { return 0.5; }
  return 1.0;
}

void renderGameState() {
  // Check the buttons (we render these all the time, they are just sometimes ignored)
  setButtonIndicator(redButton, RED_BUTTON_INDICATOR_PIN);
  setButtonIndicator(greenButton, GREEN_BUTTON_INDICATOR_PIN);

  // Step the rainbow
  switch (currentGameState) {
    case STATE_GAME_START:
      // Taste the rainbow while you wait...
      rainbow(50);
      break;

    case STATE_GUESS_START:
    case STATE_GUESS:
      // These two states are rendered identically
      for(int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
        uint32_t currentGuessColor = currentGuessColors[i] == PEG_NONE ? COLOR_BLACK : pegColors[currentGuessColors[i]];
        if (i == currentGuessIndex) {
          // This will give us a number between [0, 500), we want a percentage brightness
          //float dimLevel = 1.0 - (blinkTimer % BLINK_CYCLE_TIME / 1000.0);
          // ^^ ick... that's gives me seizures
          float dimLevel = getDimLevel();
          strip.setPixelColor(i, PXDIM(PXRED(currentGuessColor), dimLevel), PXDIM(PXGREEN(currentGuessColor), dimLevel), PXDIM(PXBLUE(currentGuessColor), dimLevel));
        }
        else {
          strip.setPixelColor(i, PXRED(currentGuessColor), PXGREEN(currentGuessColor), PXBLUE(currentGuessColor));
        }
      }
      strip.show();

      break;
  }
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
