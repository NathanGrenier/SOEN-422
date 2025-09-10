int S1_GREEN_LED = 2;
int S1_AMBER_LED = 3;
int S1_RED_LED = 4;

int WALKWAY_BTN = 10;

unsigned long S1_GREEN_LED_HIGH_TIME = 0;
unsigned long S1_AMBER_LED_HIGH_TIME = 0;
unsigned long S1_RED_LED_HIGH_TIME = 0;

int S1_GREEN_LED_DELAY = 0;
int S1_AMBER_LED_DELAY = 0;
int S1_RED_LED_DELAY = 0;

unsigned int S1CycleStartTime = 0;

void setup() {
  pinMode(S1_GREEN_LED, OUTPUT);
  pinMode(S1_AMBER_LED, OUTPUT);
  pinMode(S1_RED_LED, OUTPUT);

  pinMode(WALKWAY_BTN, INPUT);
}

void ledDigitalWrite(int id, int duration) {
  digitalWrite(id, HIGH);
  switch (id) {
    case S1_GREEN_LED:
      S1_GREEN_LED_HIGH_TIME = millis();
      S1_GREEN_LED_DELAY = duration;
      break;
    case S1_AMBER_LED:
      S1_AMBER_LED_HIGH_TIME = millis();
      S1_AMBER_LED_DELAY = duration;
      break;
    case S1_RED_LED:
      S1_RED_LED_HIGH_TIME = millis();
      S1_RED_LED_DELAY = duration;
      break;
  }
}

void turnOffLEDRoutine(unsigned long now) {  
  if (digitalRead(S1_GREEN_LED) == HIGH) {
    if (now - S1_GREEN_LED_HIGH_TIME >= S1_GREEN_LED_DELAY) {
      digitalWrite(S1_GREEN_LED, LOW);
    }
  }
  if (digitalRead(S1_AMBER_LED) == HIGH) {
    if (now - S1_AMBER_LED_HIGH_TIME >= S1_AMBER_LED_DELAY) {
      digitalWrite(S1_AMBER_LED, LOW);
    }
  }
  if (digitalRead(S1_RED_LED) == HIGH) {
    if (now - S1_RED_LED_HIGH_TIME >= S1_RED_LED_DELAY) {
      digitalWrite(S1_RED_LED, LOW);
    }
  }
}

void startLEDCycle() {
  S1CycleStart = millis();
  ledDigitalWrite(S1_RED_LED, 2000);
}

void updateLEDCycle() {
  
}

void loop() {
  unsigned long now = millis();

  turnOffLEDRoutine(now);

  buttonState = digitalRead(WALKWAY_BTN);
  
  ledDigitalWrite(S1_RED_LED, 2000);
  ledDigitalWrite(S1_AMBER_LED, 1000);
  ledDigitalWrite(S1_GREEN_LED, 500);
  delay(500);
}
