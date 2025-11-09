#include <Arduino.h>
#include <SPIFFS.h>
#include <time.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

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
const unsigned char SERVO_DATA_PIN = 13;     // yellow wire
const unsigned char PROXIMITY_DATA_PIN = 34; // white wire
const unsigned char GREEN_LED_PIN = 14;      // white wire
const unsigned char RED_LED_PIN = 12;        // blue wire
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 23,
    .dio = {/*dio0*/ 26, /*dio1*/ 33, /*dio2*/ 32},
};

// --- Global Variables ---
const unsigned char PORT = 80;
AsyncWebServer server(PORT);

// --- Servo Configuration ---
Servo servo;
const unsigned char SERVO_TIMER_ID = 0;
const unsigned char SERVO_PERIOD = 50; // in Hz
// Datasheet for Hitec HS-422: https://media.digikey.com/pdf/data%20sheets/dfrobot%20pdfs/ser0002_web.pdf
const unsigned short SERVO_MIN = 900;
const unsigned short SERVO_MAX = 2100;

const unsigned short SERVO_DOOR_CLOSED_POS = 0; // Degrees
const unsigned short SERVO_DOOR_OPEN_POS = 90;  // Degrees

// --- Distance Sensor Variables ---
// Sharp GP2Y0A02 datasheet: https://global.sharp/products/device/lineup/data/pdf/datasheet/gp2y0a02yk_e.pdf
// ESP32 ADC Docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc/index.html
const int ADC_MAX = 4095;                        // ESP32 12-bit ADC
const float ADC_VREF = 3.3;                      // ESP32 ADC reference voltage
const int PROX_NUM_READINGS = 10;                // Number of samples for averaging
const int PROX_READING_DELAY_US = 50000;         // 50ms delay between samples
const unsigned long PROX_READ_INTERVAL_MS = 500; // Read sensor every 500ms

const float PROXIMITY_THRESHOLD_CM = 50.0; // If distance < this, someone is "nearby"
const float VOLTAGE_MIN_FOR_RANGE = 0.40;  // Corresponds to ~150cm. Below this is "out of range"
const float VOLTAGE_MAX_FOR_RANGE = 2.80;  // Corresponds to ~15-20cm. Above this is "very close"

int proxReadings[PROX_NUM_READINGS];
int proxReadingIndex = 0;
long proxReadingTotal = 0;
unsigned long lastProxSampleTimeUs = 0;

unsigned long lastProxReadTime = 0;
float currentDistanceCm = -1.0; // -1.0 for unknown/out of range
volatile bool isPersonNearby = false;

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

// --- NTP (Time) Configuration ---
const char *ntpServer = "pool.ntp.org";
// POSIX TZ string for Montreal (Eastern Time with DST)
// EST5EDT = Standard time is EST (UTC-5), Daylight time is EDT
// M3.2.0 = DST starts 2nd Sunday in March
// M11.1.0 = DST ends 1st Sunday in November
const char *tzInfo = "EST5EDT,M3.2.0,M11.1.0";

// --- LoRaWAN Variables and Setup ---
static const u1_t PROGMEM APPEUI[8] = APP_EUI;
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }
static const u1_t PROGMEM DEVEUI[8] = DEV_EUI;
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }
static const u1_t PROGMEM APPKEY[16] = APP_KEY;
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }

struct LoRaLogPayload
{
  uint32_t timestamp; // 4 bytes for Unix time
  uint8_t success;    // 1 byte (0 = fail, 1 = success)
  char username[8];   // 8 bytes for a truncated username
};

static uint8_t mydata[] = "Hello, world!";
static osjob_t sendjob;

const unsigned TX_INTERVAL = 60; // in seconds

void printHex2(unsigned v)
{
  v &= 0xff;
  if (v < 16)
    Serial.print('0');
  Serial.print(v, HEX);
}

/**
 * @brief Prepares and queues a structured login attempt payload for LoRaWAN.
 * @param username The username string from the login attempt.
 * @param isSuccess True if the login was successful, false otherwise.
 */
void sendLoginAttemptLog(String username, bool isSuccess)
{
  // Check if LoRaWAN is busy
  if (LMIC.opmode & OP_TXRXPEND)
  {
    Serial.println(F("OP_TXRXPEND, not sending login log"));
    return;
  }

  LoRaLogPayload payload;

  // Get current time
  time_t now;
  time(&now);
  payload.timestamp = (uint32_t)now;

  // Set success status
  payload.success = isSuccess ? 1 : 0;

  // Set username (and truncate)
  // Clear the buffer with nulls
  memset(payload.username, 0, sizeof(payload.username));
  // Copy up to 7 chars to leave room for a null terminator
  strncpy(payload.username, username.c_str(), sizeof(payload.username) - 1);

  // Queue the packet
  LMIC_setTxData2(1, (uint8_t *)&payload, sizeof(payload), 0);

  Serial.printf("[INFO] Login log queued: User=%s, Success=%d, Time=%u\n",
                payload.username, payload.success, payload.timestamp);
}

void onEvent(ev_t ev)
{
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev)
  {
  case EV_SCAN_TIMEOUT:
    Serial.println(F("EV_SCAN_TIMEOUT"));
    break;
  case EV_BEACON_FOUND:
    Serial.println(F("EV_BEACON_FOUND"));
    break;
  case EV_BEACON_MISSED:
    Serial.println(F("EV_BEACON_MISSED"));
    break;
  case EV_BEACON_TRACKED:
    Serial.println(F("EV_BEACON_TRACKED"));
    break;
  case EV_JOINING:
    Serial.println(F("EV_JOINING"));
    break;
  case EV_JOINED:
    Serial.println(F("EV_JOINED"));
    {
      u4_t netid = 0;
      devaddr_t devaddr = 0;
      u1_t nwkKey[16];
      u1_t artKey[16];
      LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
      Serial.print("netid: ");
      Serial.println(netid, DEC);
      Serial.print("devaddr: ");
      Serial.println(devaddr, HEX);
      Serial.print("AppSKey: ");
      for (size_t i = 0; i < sizeof(artKey); ++i)
      {
        if (i != 0)
          Serial.print("-");
        printHex2(artKey[i]);
      }
      Serial.println("");
      Serial.print("NwkSKey: ");
      for (size_t i = 0; i < sizeof(nwkKey); ++i)
      {
        if (i != 0)
          Serial.print("-");
        printHex2(nwkKey[i]);
      }
      Serial.println();
    }
    // Disable link check validation (automatically enabled
    // during join, but because slow data rates change max TX
    // size, we don't use it in this example.
    LMIC_setLinkCheckMode(0);
    break;

  case EV_RFU1:
    Serial.println(F("EV_RFU1"));
    break;

  case EV_JOIN_FAILED:
    Serial.println(F("EV_JOIN_FAILED"));
    break;
  case EV_REJOIN_FAILED:
    Serial.println(F("EV_REJOIN_FAILED"));
    break;
  case EV_TXCOMPLETE:
    Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
    if (LMIC.txrxFlags & TXRX_ACK)
      Serial.println(F("Received ack"));
    if (LMIC.dataLen)
    {
      Serial.print(F("Received "));
      Serial.print(LMIC.dataLen);
      Serial.println(F(" bytes of payload"));
    }
    // Schedule next transmission
    // os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
    break;
  case EV_LOST_TSYNC:
    Serial.println(F("EV_LOST_TSYNC"));
    break;
  case EV_RESET:
    Serial.println(F("EV_RESET"));
    break;
  case EV_RXCOMPLETE:
    // data received in ping slot
    Serial.println(F("EV_RXCOMPLETE"));
    break;
  case EV_LINK_DEAD:
    Serial.println(F("EV_LINK_DEAD"));
    break;
  case EV_LINK_ALIVE:
    Serial.println(F("EV_LINK_ALIVE"));
    break;
  case EV_SCAN_FOUND:
    Serial.println(F("EV_SCAN_FOUND"));
    break;
  case EV_TXSTART:
    Serial.println(F("EV_TXSTART"));
    break;
  case EV_TXCANCELED:
    Serial.println(F("EV_TXCANCELED"));
    break;
  case EV_RXSTART:
    /* do not print anything -- it wrecks timing */
    break;
  case EV_JOIN_TXCOMPLETE:
    Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
    break;

  default:
    Serial.print(F("Unknown event: "));
    Serial.println((unsigned)ev);
    break;
  }
}

void initNTP()
{
  Serial.println("Configuring time from NTP...");
  // Use configTzTime for automatic DST handling
  configTzTime(tzInfo, ntpServer);

  Serial.println("Waiting for time synchronization...");

  // Wait until time is synchronized
  struct tm timeinfo;
  // 10-second timeout
  if (!getLocalTime(&timeinfo, 10000))
  {
    Serial.println("Failed to obtain time from NTP.");
  }
  else
  {
    Serial.println("Time synchronized successfully.");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S %Z");
  }
}

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

/**
 * @brief Converts a raw ADC value (0-4095) to a voltage (0-3.3V).
 * @param rawValue The raw ADC value.
 * @return The corresponding voltage.
 */
float rawToVoltage(int rawValue)
{
  return (rawValue / (float)ADC_MAX) * ADC_VREF;
}

/**
 * @brief Converts sensor voltage to distance in cm.
 * Based on fitting the inverse-distance-to-voltage graph from the datasheet.
 * Formula: L = 48.96 / (V - 0.152)
 * This formula is valid for the 20cm-150cm range.
 * @param voltage The sensor output voltage.
 * @return Distance in cm, or -1.0 if out of the reliable range.
 */
float voltageToDistanceCm(float voltage)
{
  if (voltage < VOLTAGE_MIN_FOR_RANGE)
  {
    return -1.0; // Out of range (> 150cm)
  }

  if (voltage > VOLTAGE_MAX_FOR_RANGE)
  {
    return 15.0; // Very close (< 20cm) - return a fixed "close" value
  }

  float distance = 48.96 / (voltage - 0.152);

  // Clamp to the sensor's effective range
  if (distance < 20.0)
    return 20.0;
  if (distance > 150.0)
    return 150.0;

  return distance;
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
    Serial.println("\n[INFO] Login attempt received.");
    // Calculate the current average raw value from the total
    int avgRaw = proxReadingTotal / PROX_NUM_READINGS;
    // Print the sensor's current state
    Serial.printf("[INFO] Proximity Data | Raw: %d, Volt: %.2fV, Dist: %.1fcm, Nearby: %s\n",
                  avgRaw,
                  rawToVoltage(avgRaw),
                  currentDistanceCm,
                  isPersonNearby ? "YES" : "NO");
    
    if (isLockedOut()) {
      request->send(403, "text/plain", "Locked out. Try again later.");
      return;
    }

    if (request->hasParam("USERNAME", true) && request->hasParam("PASSWORD", true)) {
      String user = request->getParam("USERNAME", true)->value();
      String pass = request->getParam("PASSWORD", true)->value();

      if (user == roomUsername && pass == roomPassword) {
        loginSuccessTrigger = true;
        if (isPersonNearby) {
          // REQ: Correct creds + Person detected
          request->send(200, "text/plain", "Login Successful! Door opening.");
          sendLoginAttemptLog(user, true);
        } else {
          // REQ: Correct creds + NO Person
          request->send(401, "text/plain", "Login successful, but no person detected. Please stand closer.");
          sendLoginAttemptLog(user, false);
        }
      } else {
        loginFailTrigger = true;
        sendLoginAttemptLog(user, false);
        
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

  // --- Proximity Sensor Setup ---
  pinMode(PROXIMITY_DATA_PIN, INPUT);
  // Initialize rolling average array
  for (int i = 0; i < PROX_NUM_READINGS; i++)
  {
    proxReadings[i] = 0;
  }

  connectToWifi();
  initNTP();

  setupWebServer();

  // --- LoRaWAN Setup ---
  os_init();    // LMIC init
  LMIC_reset(); // Reset the MAC state. Session and pending data transfers will be discarded.
  Serial.println("[INFO] Queuing LoRaWAN startup message...");
  sendLoginAttemptLog("system", true);

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

      if (isPersonNearby)
      {
        // REQ: Correct creds + Person detected = GREEN LED + Open Door
        RA_State = DOOR_OPENING;
      }
      else
      {
        // REQ: Correct creds + NO Person = RED LED
        // We transition to FAILED_ATTEMPT to show the red light, but we dont't incremented incorrectAttempts.
        RA_State = FAILED_ATTEMPT;
      }
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
        // REQ: Incorrect creds + (Person OR NO Person) = RED LED
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
  os_runloop_once();

  // --- Non-blocking state machine tick ---
  if (RA_Tick)
  {
    RA_Tick = false;
    TickFct_RoomAccess();
  }

  // --- Non-blocking Proximity Sampling ---
  if (micros() - lastProxSampleTimeUs >= PROX_READING_DELAY_US)
  {
    lastProxSampleTimeUs = micros();

    // Subtract the oldest reading from the total
    proxReadingTotal -= proxReadings[proxReadingIndex];

    // Take a new reading and add it to the total
    int newReading = analogRead(PROXIMITY_DATA_PIN);
    proxReadings[proxReadingIndex] = newReading;
    proxReadingTotal += newReading;

    proxReadingIndex++;
    // Wrap around the index if needed
    if (proxReadingIndex >= PROX_NUM_READINGS)
    {
      proxReadingIndex = 0;
    }
  }

  // --- Non-blocking Proximity Calculation ---
  if (millis() - lastProxReadTime >= PROX_READ_INTERVAL_MS)
  {
    lastProxReadTime = millis();

    // Calculate the average from the rolling total
    int avgRaw = proxReadingTotal / PROX_NUM_READINGS;

    float voltage = rawToVoltage(avgRaw);
    currentDistanceCm = voltageToDistanceCm(voltage);

    // Update the nearby flag
    if (currentDistanceCm > 0 && currentDistanceCm < PROXIMITY_THRESHOLD_CM)
    {
      isPersonNearby = true;
    }
    else
    {
      isPersonNearby = false;
    }

    // Uncomment for debugging
    // Serial.printf("Raw: %d, Volt: %.2fV, Dist: %.1fcm, Nearby: %s\n",
    //               avgRaw, voltage, currentDistanceCm, isPersonNearby ? "YES" : "NO");
    // Serial.print("proxReadings: [");
    // for (int i = 0; i < PROX_NUM_READINGS; ++i)
    // {
    //   Serial.print(proxReadings[i]);
    //   if (i < PROX_NUM_READINGS - 1)
    //     Serial.print(", ");
    // }
    // Serial.println("]");
  }
}