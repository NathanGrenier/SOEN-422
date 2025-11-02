#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

#include "credentials.h"

// --- WiFi Credentials ---
const char *SSID = WIFI_SSID;
const char *PASSWORD = WIFI_PASSWORD;

// --- Room Access Credentials ---
String roomUsername = ROOM_USERNAME;
String roomPassword = ROOM_PASSWORD;

// --- Admin Access Credentials ---

const char *ADMIN_USERNAME = ADMIN_USERNAME_CREDENTIALS;
const char *ADMIN_PASSWORD = ADMIN_PASSWORD_CREDENTIALS;
const char *ADMIN_REALM = "Admin Panel";

// --- Hardware Pin Definitions ---
const unsigned char SERVO_DATA_PIN = 13; // yellow wire
const unsigned char GREEN_LED_PIN = 14;  // white wire
const unsigned char RED_LED_PIN = 12;    // blue wire

// --- Global Variables ---
const unsigned char PORT = 80;
AsyncWebServer server(PORT);

Servo servo;
const unsigned char SERVO_TIMER_ID = 0;
const unsigned char SERVO_PERIOD = 50; // in Hz
// Datasheet for Hitec HS-422: https://media.digikey.com/pdf/data%20sheets/dfrobot%20pdfs/ser0002_web.pdf
const unsigned short SERVO_MIN = 900;
const unsigned short SERVO_MAX = 2100;

const unsigned short SERVO_DOOR_CLOSED_POS = 0; // Degrees
const unsigned short SERVO_DOOR_OPEN_POS = 90;  // Degrees

// --- Login & Lockout Logic Variables ---
volatile int incorrectAttempts = 0;
const unsigned char MAX_FAILED_ATTEMPTS = 3;
const unsigned long LOCKOUT_DURATION_MS = 2 * 60 * 1000; // 2 minutes
const unsigned long DOOR_OPEN_DURATION_MS = 5000;        // 5 seconds
const unsigned long LED_ON_DURATION_MS = 2000;           // 2 seconds
volatile unsigned long remainingLockoutTimeMs = 0;

// --- State Machine and Timer Variables ---
hw_timer_t *RA_Timer = NULL;
const unsigned char RA_TIMER_ID = 1;
const unsigned short RA_PERIOD_US = 1000;                // Timer period in microseconds (1000us = 1ms)
const unsigned short RA_PERIOD_MS = RA_PERIOD_US / 1000; // Period in milliseconds
volatile bool RA_Tick = false;
bool RA_TimerEnabled = false;

void IRAM_ATTR RA_TimerISR()
{
  RA_Tick = true;
}

enum RA_States
{
  IDLE,
  DOOR_OPENING,
  DOOR_OPEN,
  DOOR_CLOSING,
  FAILED_ATTEMPT,
  FAILED_COOLDOWN,
  LOCKED_OUT,
  LOCKED_OUT_WAIT
};

RA_States RA_State = IDLE;
unsigned long stateTimer = 0;

volatile bool loginSuccessTrigger = false;
volatile bool loginFailTrigger = false;

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

bool isAdminAuthenticated(AsyncWebServerRequest *request)
{
  if (!request->authenticate(ADMIN_USERNAME, ADMIN_PASSWORD))
  {
    request->requestAuthentication(ADMIN_REALM, false);
    return false;
  }
  return true;
}

bool isLockedOut()
{
  return RA_State == LOCKED_OUT || RA_State == LOCKED_OUT_WAIT;
}

void setupWebServer()
{
  // --- Serve static files from SPIFFS ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (isLockedOut()) {
      request->redirect("/cooldown.html");
    } else {
      request->send(SPIFFS, "/index.html", "text/html");
    } });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/script.js", "text/javascript"); });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request)
            {  
    // Basic Authentication
    if (!isAdminAuthenticated(request)) {
      return;
    }    
    request->send(SPIFFS, "/admin.html", "text/html"); });

  server.on("/admin.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
    if (!isAdminAuthenticated(request)) {
      return;
    }    
    request->send(SPIFFS, "/admin.js", "text/javascript"); });

  server.on("/cooldown.html", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/cooldown.html", "text/html"); });

  server.on("/cooldown.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/cooldown.js", "text/javascript"); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  // --- Handle API Requests ---
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (isLockedOut()) {
      request->send(403, "text/plain", "Locked out. Try again later.");
      return;
    }

    if (request->hasParam("USERNAME", true) && request->hasParam("PASSWORD", true)) {
      String user = request->getParam("USERNAME", true)->value();
      String pass = request->getParam("PASSWORD", true)->value();

      if (user == roomUsername && pass == roomPassword) {
        loginSuccessTrigger = true;
        request->send(200, "text/plain", "Login Successful! Door opening.");
      } else {
        loginFailTrigger = true;
        
        int attemptsLeft = MAX_FAILED_ATTEMPTS - (incorrectAttempts + 1);
        if (attemptsLeft <= 0) {
          request->send(403, "text/plain", String(MAX_FAILED_ATTEMPTS) + " failed attempts. Locked out for " + String(LOCKOUT_DURATION_MS / (60 * 1000)) +" minutes.");
        } else {
          String msg = "Invalid credentials. " + String(attemptsLeft) + " attempt(s) remaining.";
          request->send(401, "text/plain", msg);
        }
      }
    } else {
      request->send(400, "text/plain", "Missing username or password.");
    } });

  server.on("/update-credentials", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (!isAdminAuthenticated(request)) {
      return;
    }    
    
    if (request->hasParam("OLD_PASSWORD", true) && request->hasParam("NEW_USERNAME", true) && request->hasParam("NEW_PASSWORD", true)) {
      
      String oldPass = request->getParam("OLD_PASSWORD", true)->value();
      
      if (oldPass != roomPassword) {
        request->send(401, "text/plain", "Incorrect current password. Credentials not updated.");
        return;
      }

      roomUsername = request->getParam("NEW_USERNAME", true)->value();
      roomPassword = request->getParam("NEW_PASSWORD", true)->value();

      Serial.println("Credentials Updated!");
      Serial.println("New User: " + roomUsername);
      Serial.println("New Pass: " + roomPassword);
      
      request->send(200, "text/plain", "Success! Credentials have been updated.");

    } else {
      request->send(400, "text/plain", "Missing required fields.");
    } });

  server.on("/cooldown-time", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("[INFO] Cooldown time requested: " + String(remainingLockoutTimeMs) + " ms remaining.");
    if (isLockedOut()) {
      request->send(200, "text/plain", String(remainingLockoutTimeMs));
    } else {
      // Not locked out, send 0
      request->send(200, "text/plain", "0");
    } });

  // --- Other Paths ---
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->redirect("/"); });

  server.begin();
  Serial.println(String("HTTP server started: Accessible at http://") + WiFi.localIP().toString() + ":" + String(PORT));
}

void enableTimer(hw_timer_t *timer, bool &isEnabledFlag)
{
  if (!isEnabledFlag)
  {
    timerAlarmEnable(timer);
    isEnabledFlag = true;
  }
}

void disableTimer(hw_timer_t *timer, bool &isEnabledFlag)
{
  if (isEnabledFlag)
  {
    timerAlarmDisable(timer);
    isEnabledFlag = false;
  }
}

void closeDoor();

void setup()
{
  // Wait for Serial to be ready
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  // --- Initialize SPIFFS ---
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  else
  {
    unsigned long total = (unsigned long)SPIFFS.totalBytes();
    unsigned long used = (unsigned long)SPIFFS.usedBytes();
    Serial.printf("SPIFFS total: %lu bytes\nSPIFFS used : %lu bytes\nSPIFFS free : %lu bytes\n", total, used, total - used);
  }

  // --- LED Setup ---
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // --- Servo Setup ---
  ESP32PWM::allocateTimer(SERVO_TIMER_ID);
  servo.setPeriodHertz(SERVO_PERIOD);
  servo.attach(SERVO_DATA_PIN, SERVO_MIN, SERVO_MAX);

  connectToWifi();

  setupWebServer();

  closeDoor(); // Ensure door is closed at startup

  // Initialize State Machine Timer ---
  // Specify Timer ID, prescaler 80 (for 1MHz, 1us ticks), count up.
  RA_Timer = timerBegin(RA_TIMER_ID, 80, true);
  // Attach the ISR to the timer
  timerAttachInterrupt(RA_Timer, &RA_TimerISR, true);
  // Set alarm to trigger every PERIOD, and auto-reload
  timerAlarmWrite(RA_Timer, RA_PERIOD_US, true);
  // Start the timer's alarm
  enableTimer(RA_Timer, RA_TimerEnabled);

  // Serial.println("Heap Memory After Setup:");
  // Serial.printf("Free Heap: %u bytes, Max Contiguous Block: %u bytes\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

void openDoor()
{
  Serial.println("[INFO] Opening door...");
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  servo.write(SERVO_DOOR_OPEN_POS);
}

void closeDoor()
{
  Serial.println("[INFO] Closing door...");
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  servo.write(SERVO_DOOR_CLOSED_POS);
}

void signalFailedAttempt()
{
  Serial.println("[INFO] Failed attempt.");
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
}

void clearLEDIndicators()
{
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
}

void TickFct_RoomAccess()
{
  // --- State Transitions ---
  switch (RA_State)
  {
  case IDLE:
    if (loginSuccessTrigger)
    {
      loginSuccessTrigger = false;
      incorrectAttempts = 0;
      RA_State = DOOR_OPENING;
    }
    else if (loginFailTrigger)
    {
      loginFailTrigger = false;
      incorrectAttempts++;

      if (incorrectAttempts >= MAX_FAILED_ATTEMPTS)
      {
        RA_State = LOCKED_OUT;
      }
      else
      {
        RA_State = FAILED_ATTEMPT;
      }
    }
    break;

  case DOOR_OPENING:
    stateTimer = 0;
    RA_State = DOOR_OPEN;
    break;

  case DOOR_OPEN:
    stateTimer += RA_PERIOD_MS;
    if (stateTimer >= DOOR_OPEN_DURATION_MS)
    {
      Serial.println("[INFO] Door open duration elapsed.");
      RA_State = DOOR_CLOSING;
    }
    break;

  case DOOR_CLOSING:
    RA_State = IDLE;
    break;

  case FAILED_ATTEMPT:
    stateTimer = 0;
    RA_State = FAILED_COOLDOWN;
    break;

  case FAILED_COOLDOWN:
    stateTimer += RA_PERIOD_MS;
    if (stateTimer >= LED_ON_DURATION_MS)
    {
      RA_State = IDLE;
    }
    break;

  case LOCKED_OUT:
    incorrectAttempts = 0; // Reset counter
    stateTimer = 0;
    remainingLockoutTimeMs = LOCKOUT_DURATION_MS;
    RA_State = LOCKED_OUT_WAIT;
    break;

  case LOCKED_OUT_WAIT:
    stateTimer += RA_PERIOD_MS;
    if (stateTimer >= LOCKOUT_DURATION_MS)
    {
      remainingLockoutTimeMs = 0;
      RA_State = IDLE;
      Serial.println("[INFO] Locked out cooldown complete.");
    }
    else
    {
      remainingLockoutTimeMs = LOCKOUT_DURATION_MS - stateTimer;
    }
    break;

  default:
    Serial.println("[ERROR] Unknown state transition!");
    break;
  }

  // --- State Actions ---
  switch (RA_State)
  {
  case IDLE:
    // Clear remaining time just in case
    if (remainingLockoutTimeMs > 0)
    {
      remainingLockoutTimeMs = 0;
    }

    // If we just entered IDLE from a cooldown, turn off the light
    if (digitalRead(RED_LED_PIN) == HIGH || digitalRead(GREEN_LED_PIN) == HIGH)
    {
      clearLEDIndicators();
    }
    break;

  case DOOR_OPENING:
    openDoor();
    break;

  case DOOR_OPEN:
    break;

  case DOOR_CLOSING:
    closeDoor();
    break;

  case FAILED_ATTEMPT:
    signalFailedAttempt();
    break;

  case FAILED_COOLDOWN:
    break;

  case LOCKED_OUT:
    signalFailedAttempt();
    Serial.println("[INFO] Device locked out.");
    break;

  case LOCKED_OUT_WAIT:
    break;

  default:
    Serial.println("[ERROR] Unknown state action!");
    break;
  }
}

void loop()
{
  // Non-blocking state machine tick
  if (RA_Tick)
  {
    RA_Tick = false;
    TickFct_RoomAccess();
  }
}