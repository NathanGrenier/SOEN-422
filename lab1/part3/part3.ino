/**
 * SOEN422 – Fall 2025: Embedded Systems/Software
 * Section MM/MI-X - Lab Assignment 1: Part 3
 *
 * Authors: Nathan Grenier, Giuliano Verdone
 */

// Global constants for Arduino Nano I/O Pin Configuration
const unsigned char S1_GREEN_LED = 2;
const unsigned char S1_AMBER_LED = 3;
const unsigned char S1_RED_LED = 4;

const unsigned char S2_GREEN_LED = 5;
const unsigned char S2_AMBER_LED = 6;
const unsigned char S2_RED_LED = 7;

const unsigned char WALKWAY_BTN = 10;

// Define LED Color Duration Constants (in milliseconds)
const unsigned int RED_DURATION = 5000;
const unsigned int AMBER_DURATION = 1000;
const unsigned int GREEN_DURATION = 4000;

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
    unsigned char pin;
    unsigned int duration;
    LEDNode *next;
    LEDNode *prev;
};

/**
 * Represents a traffic light system.
 * Each traffic light contains the following:
 * - currentLED: pointer to the currently active LED node in a doubly linked list
 * - currentLEDStartTime: the time when the current LED was turned on (relative to the beginning of a record time)
 */
struct TrafficLight
{
    LEDNode *currentLED;
    unsigned long currentLEDStartTime;
};

LEDNode S1Red, S1Amber, S1Green;
LEDNode S2Red, S2Amber, S2Green;

TrafficLight S1TrafficLight;
TrafficLight S2TrafficLight;

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

    pinMode(S2_GREEN_LED, OUTPUT);
    pinMode(S2_AMBER_LED, OUTPUT);
    pinMode(S2_RED_LED, OUTPUT);

    pinMode(WALKWAY_BTN, INPUT);

    // Create LED nodes for TrafficLights
    S1Red = {S1_RED_LED, RED_DURATION, &S1Green, &S1Amber};
    S1Amber = {S1_AMBER_LED, AMBER_DURATION, &S1Red, &S1Green};
    S1Green = {S1_GREEN_LED, GREEN_DURATION, &S1Amber, &S1Red};

    S2Red = {S2_RED_LED, RED_DURATION, &S2Green, &S2Amber};
    S2Amber = {S2_AMBER_LED, AMBER_DURATION, &S2Red, &S2Green};
    S2Green = {S2_GREEN_LED, GREEN_DURATION, &S2Amber, &S2Red};

    // S1 Traffic Light Starts on Red
    S1TrafficLight = {&S1Red, 0};
    // S2 Traffic Light Starts on Green
    S2TrafficLight = {&S2Green, 0};

    // Initially, turn on the starting LEDs of both traffic lights
    unsigned long now = millis();

    digitalWrite(S1TrafficLight.currentLED->pin, HIGH);
    S1TrafficLight.currentLEDStartTime = now;

    digitalWrite(S2TrafficLight.currentLED->pin, HIGH);
    S2TrafficLight.currentLEDStartTime = now;
}

void updateTrafficLight(TrafficLight &trafficLight, unsigned long now)
{
    // Check if the LED should be turned OFF
    if (now - trafficLight.currentLEDStartTime >= trafficLight.currentLED->duration)
    {
        // Turn the current LED OFF
        digitalWrite(trafficLight.currentLED->pin, LOW);

        // Update start time for the next light by the fixed duration to prevent drift
        trafficLight.currentLEDStartTime += trafficLight.currentLED->duration;

        // Move to the next node in the sequence
        trafficLight.currentLED = trafficLight.currentLED->next;

        // Turn the new current LED ON immediately
        digitalWrite(trafficLight.currentLED->pin, HIGH);
    }
}

bool detectButtonPress()
{
    bool buttonState = digitalRead(WALKWAY_BTN);
    if (buttonState == HIGH && lastButtonState == LOW)
    {
        lastButtonState = buttonState;
        return true;
    }
    lastButtonState = buttonState;
    return false;
}

void loop()
{
    unsigned long now = millis();

    if (detectButtonPress())
    {
        // If the button is pressed while S1 is Green, S1 immediately switches to Amber.
        // S2 immediately switches to Red with a remaining duration equal to the AMBER_DURATION.
        if (S1TrafficLight.currentLED == &S1Green)
        {
            // Turn off current LEDs
            digitalWrite(S1TrafficLight.currentLED->pin, LOW);
            digitalWrite(S2TrafficLight.currentLED->pin, LOW);

            // Switch S1 to Amber
            S1TrafficLight.currentLED = &S1Amber;
            digitalWrite(S1TrafficLight.currentLED->pin, HIGH);
            S1TrafficLight.currentLEDStartTime = now;

            // Switch S2 to Red - Amber LED duration
            S2TrafficLight.currentLED = &S2Red;
            digitalWrite(S2TrafficLight.currentLED->pin, HIGH);
            S2TrafficLight.currentLEDStartTime = now - (RED_DURATION - AMBER_DURATION);
        }
    }

    updateTrafficLight(S1TrafficLight, now);
    updateTrafficLight(S2TrafficLight, now);
}