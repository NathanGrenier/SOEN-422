// Global constants for Arduino Nano I/O Pin Configuration
const int S1_GREEN_LED = 2;
const int S1_AMBER_LED = 3;
const int S1_RED_LED = 4;

const int S2_GREEN_LED = 5;
const int S2_AMBER_LED = 6;
const int S2_RED_LED = 7;

void setup()
{
    pinMode(S1_GREEN_LED, OUTPUT);
    pinMode(S1_AMBER_LED, OUTPUT);
    pinMode(S1_RED_LED, OUTPUT);

    pinMode(S2_GREEN_LED, OUTPUT);
    pinMode(S2_AMBER_LED, OUTPUT);
    pinMode(S2_RED_LED, OUTPUT);
}

void loop()
{
    digitalWrite(S1_GREEN_LED, HIGH);
    digitalWrite(S1_AMBER_LED, HIGH);
    digitalWrite(S1_RED_LED, HIGH);

    digitalWrite(S2_GREEN_LED, HIGH);
    digitalWrite(S2_AMBER_LED, HIGH);
    digitalWrite(S2_RED_LED, HIGH);

    delay(1000);

    digitalWrite(S1_GREEN_LED, LOW);
    digitalWrite(S1_AMBER_LED, LOW);
    digitalWrite(S1_RED_LED, LOW);

    digitalWrite(S2_GREEN_LED, LOW);
    digitalWrite(S2_AMBER_LED, LOW);
    digitalWrite(S2_RED_LED, LOW);

    delay(1000);
}