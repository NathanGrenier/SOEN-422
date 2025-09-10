int S1_GREEN_LED = 2;
int S1_AMBER_LED = 3;
int S1_RED_LED = 4;

int S2_GREEN_LED = 2;
int S2_AMBER_LED = 3;
int S2_RED_LED = 4;

void setup() {
  pinMode(S1_GREEN_LED, OUTPUT);
  pinMode(S1_AMBER_LED, OUTPUT);
  pinMode(S1_RED_LED, OUTPUT);
  
  pinMode(S2_GREEN_LED, OUTPUT);
  pinMode(S2_AMBER_LED, OUTPUT);
  pinMode(S2_RED_LED, OUTPUT);
}

void ledDigitalWrite(int id, int duration) {
  digitalWrite(id, HIGH);
  delay(duration);
  digitalWrite(id, LOW);
}

void loop() {
  ledDigitalWrite(S1_RED_LED, 2000);
  ledDigitalWrite(S1_AMBER_LED, 1000);
  ledDigitalWrite(S1_GREEN_LED, 500);
  delay(500);
}
