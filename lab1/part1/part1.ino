/**
 * SOEN422 – Fall 2025: Embedded Systems/Software
 * Section MM/MI-X - Lab Assignment 1: Part 1
 *
 * The sketch cycles through three LEDs (Red, Amber, Green) that represent a traffic light set.
 * Traversal order is dictated by the current button state (where LOW = forward, and HIGH = reverse).
 *
 * Authors: Nathan Grenier, Giuliano Verdone
 */

// Global constants for Arduino Nano I/O Pin Configuration
const int S1_GREEN_LED = 2;
const int S1_AMBER_LED = 3;
const int S1_RED_LED = 4;
const int WALKWAY_BTN = 10;
// end of Global constants for Arduino Nano I/O Pin Configuration

/**
 * Represents a LED node in the doubly linked list.
 * Each node contains the following:
 * - pin: the pin connected to the LED
 * - duration: how long the LED should stay on (in milliseconds)
 * - next: pointer to the next node in the sequence
 * - prev: pointer to the previous node
 */
struct LEDNode
{
  int pin;
  int duration;
  LEDNode *next;
  LEDNode *prev;
};

// The three elements of the traffic light set can each be represented by a LEDNode
LEDNode red, amber, green;

// The cycle state has been designed as follows:
LEDNode *current;               // Pointer to the currently active LED node
bool forward = true;            // Represents the direction of LED cycle: true = forward, false = reverse
bool ledOn = false;             // Represents whether the current LED is actually turned on
unsigned long ledStartTime = 0; // The time when the current LED was turned on (relative to the beginning of a record time)

// For comparing detection updates, a boolean variable makes it possible to keep track of the previous button state:
bool lastButtonState = LOW;

/**
 * Arduino setup function.
 *
 * Initializes pin modes and constructs the linked list of LED nodes.
 */
void setup()
{
  pinMode(S1_GREEN_LED, OUTPUT);
  pinMode(S1_AMBER_LED, OUTPUT);
  pinMode(S1_RED_LED, OUTPUT);
  pinMode(WALKWAY_BTN, INPUT);

  // Create doubly linked list of LEDs
  red = {S1_RED_LED, 2000, &amber, &green};
  amber = {S1_AMBER_LED, 1000, &green, &red};
  green = {S1_GREEN_LED, 500, &red, &amber};

  // According to the lab instructions, by default the cycle must start with red LED
  current = &red;
}

/**
 * Arduino main loop.
 *
 * Handles button press detection to toggle direction.
 * Controls the LED timing and transitions through the doubly linked list.
 */
void loop()
{
  unsigned long now = millis();

  // Upon successfull detection of a valid button press, the LED cycle gets toggled
  bool buttonState = digitalRead(WALKWAY_BTN);
  if (buttonState == HIGH && lastButtonState == LOW)
  {
    forward = !forward;
  }
  lastButtonState = buttonState;

  // The LEDs timings and transitions require control:
  if (!ledOn)
  {
    // Turn the current LED ON and record the start time
    digitalWrite(current->pin, HIGH);
    ledStartTime = now;
    ledOn = true;
  }
  else
  {
    // Check if the LED should be turned OFF
    if (now - ledStartTime >= current->duration)
    {
      digitalWrite(current->pin, LOW);
      ledOn = false;

      if (current->pin == S1_GREEN_LED) {
        delay(500);
      }
      
      // After the current LED has been turned OFF, move to next node w.r.t. the current direction
      current = forward ? current->next : current->prev;
    }
  }
}