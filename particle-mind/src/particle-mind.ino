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
void updateServerGameState();
void renderGameState();
const char *getEncodedGuess();

void processGameSubmissionEvent(const char *, const char *);
void processGameStartedEvent(const char *, const char *);
void processGuessSubmissionResult(const char *, const char *);
void processGameCompleteEvent(const char *, const char *);

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
uint32_t currentGuessResult[MAX_GUESS_ELEMENTS] = { COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK };

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
  STATE_CONFIRM_GUESS,
  STATE_SUBMIT_GUESS,
  STATE_SHOW_GUESS_RESULT,
  STATE_GAME_COMPLETE,
  STATE_GAME_COMPLETE_WINNER,
  STATE_SERVER_SET_PATTERN,
  STATE_SERVER_CONFIRM_PATTERN,
  STATE_SERVER_WAIT_FOR_GUESSES,
  STATE_SERVER_GAME_COMPLETE,
  STATE_SERVER_SHOW_WIN_PATTERN
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

  // Subscribe to game related events (subscribe to different events, depending on role)
  // TODO: Add events/handlers for ending the game and starting a new one
  #if PLATFORM_ID == PLATFORM_ARGON
  Mesh.subscribe("game-guess-submitted", processGameSubmissionEvent);
  #else
  Mesh.subscribe("game-started", processGameStartedEvent);
  Mesh.subscribe("guess-submission-result", processGuessSubmissionResult);
  Mesh.subscribe("game-complete", processGameCompleteEvent);
  #endif
  
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

#if PLATFORM_ID == PLATFORM_ARGON
  updateServerGameState();
#else
  updateGameState();
#endif

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

void updateServerGameState() {
  switch (currentGameState) {
    case STATE_GAME_START:
      // Reset Currently Selected Pattern Information
      for (int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
        currentGuessColors[i] = PEG_NONE;
      }
      numberOfGuesses = 0;

      // If the user presses a button...
      if (changeColorButtonClicked || changeIndexButtonClicked) {
        // We always start with the first item
        currentGuessIndex = 0;
        currentGuessColors[currentGuessIndex] = 0;
        blinkTimer = 0;

        // Move into setting up the pattern
        currentGameState = STATE_SERVER_SET_PATTERN;
      }

      break;

    case STATE_SERVER_SET_PATTERN:
        if (changeColorButtonClicked) {
          changeColorButtonClicked = false;
          short currentGuess = currentGuessColors[currentGuessIndex];
          currentGuessColors[currentGuessIndex] = (currentGuess == PEG_NONE ? 0 : ++currentGuess) % PEG_COLORS;
        }
        else if (changeIndexButtonClicked) {
          changeIndexButtonClicked = false;

          if (++currentGuessIndex >= MAX_GUESS_ELEMENTS) {
            // move into 'confirm pattern' mode
            currentGameState = STATE_SERVER_CONFIRM_PATTERN;
          }
          blinkTimer = 0; // reset the blink timer for this 'peg'
        }

        break;

    case STATE_SERVER_CONFIRM_PATTERN:
        if (changeColorButtonClicked) {
          changeColorButtonClicked = false;

          // move back to pattern set mode
          currentGuessIndex = 0;
          currentGameState = STATE_SERVER_SET_PATTERN;
          blinkTimer = 0;
        }
        else if (changeIndexButtonClicked) {
          changeIndexButtonClicked = false;

          // 'Lock in' the pattern
          currentGameState = STATE_SERVER_WAIT_FOR_GUESSES;
          Mesh.publish("game-started", NULL);
        }

        break;

    case STATE_SERVER_WAIT_FOR_GUESSES:
        // We're just waiting... nothing fancy...
        break;

    case STATE_SERVER_GAME_COMPLETE:
    case STATE_SERVER_SHOW_WIN_PATTERN:
        if (changeColorButtonClicked) {
          changeColorButtonClicked = false;

          // Show the pattern matched
          blinkTimer = 0;
          currentGameState = STATE_SERVER_SHOW_WIN_PATTERN;
        }
        else if (changeIndexButtonClicked) {
          changeIndexButtonClicked = false;

          // Start a new game
          Mesh.publish("game-reset");
          // Reset Currently Selected Pattern Information
          for (int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
            currentGuessColors[i] = PEG_NONE;
          }
          numberOfGuesses = 0;
          currentGameState = STATE_SERVER_SET_PATTERN;
        }

        break;
  }
}

void updateGameState() 
{
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
        if (++currentGuessIndex >= MAX_GUESS_ELEMENTS) {
          // ask the user to confirm their guess
          currentGameState = STATE_CONFIRM_GUESS;
        }
        blinkTimer = 0;
      }

      break;

    case STATE_CONFIRM_GUESS:
      if (changeColorButtonClicked && changeIndexButtonClicked && numberOfGuesses > 0) {
        changeColorButtonClicked = false; // cancel any click
        changeIndexButtonClicked = false;
        currentGameState = STATE_SHOW_GUESS_RESULT;
      }
      else if (changeColorButtonClicked) {
        changeColorButtonClicked = false;

        // move back to guess set mode
        currentGuessIndex = 0;
        currentGameState = STATE_GUESS;
        blinkTimer = 0;
      }
      else if (changeIndexButtonClicked) {
        changeIndexButtonClicked = false;

        // 'Lock in' the guess
        currentGameState = STATE_SUBMIT_GUESS;
        numberOfGuesses++;
        Mesh.publish("game-guess-submitted", getEncodedGuess());
      }

      break;

    case STATE_SUBMIT_GUESS:
      // Nothing to do here... we're just waiting for the server...
      changeColorButtonClicked = false; // cancel any click
      changeIndexButtonClicked = false;
      break;

    case STATE_SHOW_GUESS_RESULT:
      // Handle when the user clicks either button to move back to
      // state STATE_CONFIRM_GUESS, this shows the user what they
      // just submitted...
      if (changeColorButtonClicked || changeIndexButtonClicked) {
        changeColorButtonClicked = false;
        changeIndexButtonClicked = false;

        currentGameState = STATE_CONFIRM_GUESS;
      }

      break;

    case STATE_GAME_COMPLETE:
    case STATE_GAME_COMPLETE_WINNER:
      // TODO: We should track that the game finished so we can switch amongst our last guess and graded result

      // Nothing to do here... we're just waiting for the server...
      changeColorButtonClicked = false; // cancel any click
      changeIndexButtonClicked = false;

      break;
  }
}

const char *getEncodedGuess()
{
  String output = System.deviceID() + "::";
  for(int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
    output += currentGuessColors[i];
  }

  return (const char *)output;
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
    case STATE_SERVER_GAME_COMPLETE:
    case STATE_GAME_COMPLETE_WINNER:
      // Taste the rainbow while you wait...
      rainbow(50);
      break;

    case STATE_GUESS_START:
    case STATE_GUESS:
    case STATE_CONFIRM_GUESS:
    case STATE_SERVER_SET_PATTERN:
    case STATE_SERVER_CONFIRM_PATTERN:
    case STATE_SERVER_SHOW_WIN_PATTERN:
      // These states are rendered identically
      for(int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
        uint32_t currentGuessColor = currentGuessColors[i] == PEG_NONE ? COLOR_BLACK : pegColors[currentGuessColors[i]];
        if (currentGameState == STATE_SERVER_CONFIRM_PATTERN || currentGameState == STATE_CONFIRM_GUESS || i == currentGuessIndex) {
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

    case STATE_SUBMIT_GUESS:
    {
      // TODO: Some unique rendering to indicate that we're waiting for the server's response
      // Now dim the full on value based on what part of the 'cycle' we're in
      int blinkIndex = blinkTimer % BLINK_CYCLE_TIME;
      int blinkSweetSpot = BLINK_CYCLE_TIME / MAX_GUESS_ELEMENTS;
      int blinkDimCycle = BLINK_CYCLE_TIME / MAX_GUESS_ELEMENTS / 3;

      for (int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
        int sweetSpot = i * blinkSweetSpot;
        int minSweetDim = sweetSpot - blinkDimCycle;
        int maxSweetDim = sweetSpot + blinkDimCycle;

        float indexLevel = blinkIndex >= minSweetDim && blinkIndex <= maxSweetDim
                         ? abs(sweetSpot - blinkIndex) / (float)blinkDimCycle
                         : 0.0;
        strip.setPixelColor(i, PXDIM(PXRED(COLOR_WHITE), indexLevel), PXDIM(PXGREEN(COLOR_WHITE), indexLevel), PXDIM(PXBLUE(COLOR_WHITE), indexLevel));
      }
      strip.show();

      break;
    }
    
    case STATE_SHOW_GUESS_RESULT:
    case STATE_GAME_COMPLETE:
      // Show our latest guess result
      for(int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
        uint32_t currentResultColor = currentGuessResult[i];
        strip.setPixelColor(i, PXRED(currentResultColor), PXGREEN(currentResultColor), PXBLUE(currentResultColor));
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

void processGameSubmissionEvent(const char *event, const char *data)
{
  // Grade the submission (format is <submitter>::<peg1><peg2><peg3><peg4><peg5>)
  // NOTE: Assumption is color indexes are the same between client and server
  int colorMatches = 0;
  int fullMatches = 0;
  String submission(data);

  int delimiterIndex = submission.indexOf("::");
  if (delimiterIndex > 0)
  {
    int pegCount = submission.length() - (delimiterIndex+1);
    String submitter = submission.substring(0, delimiterIndex - 1);

    if (pegCount != MAX_GUESS_ELEMENTS)
    {
      // Send error response
      Mesh.publish("guess-submission-result", String::format("%s::error,wrong-number-elements", submitter));
    }
    else
    {
      numberOfGuesses++;

      // Do the matching
      int submissionIndex = delimiterIndex + 2;
      for(int i = 0; i < MAX_GUESS_ELEMENTS; i++) 
      {
        int submissionVal = submission.substring(submissionIndex + i, 1).toInt();
        if (submissionVal == currentGuessColors[i]) 
        {
          fullMatches++;
        }
        else
        {
          for(int j = 0; j < MAX_GUESS_ELEMENTS; j++)
          {
            if (submissionVal == currentGuessColors[j])
            {
              colorMatches++;
              break;
            }
          }
        }
      }

      if (fullMatches == MAX_GUESS_ELEMENTS) 
      {
        // WE HAVE A WINNER!!!
        currentGameState = STATE_SERVER_GAME_COMPLETE;
        Mesh.publish("guess-submission-result", String::format("%s::winner", submitter));
        Mesh.publish("game-complete");
      }
      else
      {
        // Send the match response
        Mesh.publish("guess-submission-result", String::format("%s::partial,%i,%i", submitter, fullMatches, colorMatches));
      }
    }
    
  }
}

void processGameStartedEvent(const char *event, const char *data)
{
  // Easy-peasy, just switch to our new state
  if (currentGameState == STATE_GAME_START || currentGameState == STATE_GAME_COMPLETE || currentGameState == STATE_GAME_COMPLETE_WINNER) {
    currentGameState = STATE_GUESS_START;
  }

  // If the game wasn't waiting to start the game, then
  // just ignore this entirely
}

void processGuessSubmissionResult(const char *event, const char *data)
{
  // Do we care about this event?
  String eventData = (String)data;
  if (eventData.startsWith(System.deviceID())) {
    // yup, this is for us, strip off the submitter information
    eventData = eventData.substring(eventData.indexOf("::") + 2);
    if (eventData.equalsIgnoreCase("winner")) {
      currentGameState = STATE_GAME_COMPLETE_WINNER;
    }
    else {
      // Store most recent guess result
      int fullMatches = eventData.substring(0, 1).toInt();
      int colorMatches = eventData.substring(2, 1).toInt();
      for(int i = 0; i < MAX_GUESS_ELEMENTS; i++) {
        if (i < fullMatches) {
          currentGuessResult[i] = COLOR_RED;
        }
        else if (i < (fullMatches + colorMatches)) {
          currentGuessResult[i] = COLOR_WHITE;
        }
        else {
          currentGuessResult[i] = COLOR_BLACK;
        }
      }

      // switch to game mode 'STATE_SHOW_GUESS_RESULT'
      currentGameState = STATE_SHOW_GUESS_RESULT;
    }
  }
  else if (eventData.endsWith("winner")) {
      // Someone else won, let's end our game
      currentGameState = STATE_GAME_COMPLETE;
  }
}

void processGameCompleteEvent(const char *event, const char *data)
{
    // The game is done (assume we didn't win if we're not already set as the winner)
    currentGameState = currentGameState != STATE_GAME_COMPLETE_WINNER ? STATE_GAME_COMPLETE : STATE_GAME_COMPLETE_WINNER;
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
