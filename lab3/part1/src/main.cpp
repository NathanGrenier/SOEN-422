#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>

#include "credentials.h"
#include "api_config.h"

const char *SSID = WIFI_SSID;
const char *PASSWORD = WIFI_PASSWORD;

const char *WEBPAGE_USERNAME = WEBPAGE_USERNAME_CREDENTIAL;
const char *WEBPAGE_PASSWORD = WEBPAGE_PASSWORD_CREDENTIAL;

const unsigned char SERVO_DATA = 26; // yellow wire

const unsigned char GREEN_LED = 14; // white wire
const unsigned char RED_LED = 12;   // blue wire

Servo servo;
unsigned short servoPosition = 0;
// Datasheet for Hitec HS-422: https://media.digikey.com/pdf/data%20sheets/dfrobot%20pdfs/ser0002_web.pdf
const unsigned short SERVO_MIN = 900;
const unsigned short SERVO_MAX = 2100;

hw_timer_t *MB_Timer = NULL;
unsigned const char MB_TIMER_ID = 0;
unsigned const short MB_PERIOD = 1000; // microseconds (1ms)
volatile bool MB_Tick = false;
bool MB_TimerEnabled = false;

void IRAM_ATTR MB_TimerISR()
{
  MB_Tick = true;
}

enum MP_States
{
  MP_Start,
};

MP_States MP_State = MP_Start;

void connectToWifi()
{
  Serial.printf("Connecting to %s ", SSID);
  if (strcmp(SSID, "SOEN422") == 0)
  {
    IPAddress local_IP(172, 30, 140, 210);
    IPAddress gateway(172, 30, 140, 129);
    IPAddress subnet(255, 255, 255, 128);
    IPAddress primaryDNS(8, 8, 8, 8);
    IPAddress secondaryDNS(8, 8, 4, 4);
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  }
  WiFi.begin(SSID, PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - startTime > 15000) // 15 second timeout
    {
      Serial.println("\nFailed to connect to WiFi. Please check credentials or network.");
      return;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  // Wait for Serial to be ready
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // --- Servo Setup ---
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_DATA, SERVO_MIN, SERVO_MAX);

  connectToWifi();

  // Initialize State Machine Timer ---
  // Specify Timer ID, prescaler 80 (for 1MHz, 1us ticks), count up.
  // MB_Timer = timerBegin(MB_TIMER_ID, 80, true);
  // // Attach the ISR to the timer
  // timerAttachInterrupt(MB_Timer, &MB_TimerISR, true);
  // // Set alarm to trigger every PERIOD, and auto-reload
  // timerAlarmWrite(MB_Timer, MB_PERIOD, true);
  // // Start the timer's alarm
  // enableTimer(MB_Timer);
}

void enableTimer(hw_timer_t *timer)
{
  if (!MB_TimerEnabled)
  {
    timerAlarmEnable(timer);
    MB_TimerEnabled = true;
  }
}

void disableTimer(hw_timer_t *timer)
{
  if (MB_TimerEnabled)
  {
    timerAlarmDisable(timer);
    MB_TimerEnabled = false;
  }
}

void loop()
{
  // // Go to 0 degrees (Home)
  // Serial.println("Moving to 0 degrees");
  // servo.write(0);
  // delay(1500); // Wait 1.5 seconds

  // // Go to 90 degrees (Center)
  // Serial.println("Moving to 90 degrees");
  // servo.write(90);
  // delay(1500); // Wait 1.5 seconds

  // // Go to 180 degrees (Max)
  // Serial.println("Moving to 180 degrees");
  // servo.write(180);
  delay(1500); // Wait 1.5 seconds
}