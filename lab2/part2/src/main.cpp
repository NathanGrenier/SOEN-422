#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <iostream>
#include <vector>

#include "credentials.h"
#include "api_config.h"

using std::vector;

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Go to https://www.uuidgenerator.net to generate unique UUIDs
#define SERVICE_UUID "d81f1e40-a43c-4c07-b83f-71d584802b4e"
#define CHARACTERISTIC_UUID "4e44cef2-ed01-4edf-952d-25d88fe182a3"

const char *SSID = WIFI_SSID;
const char *PASSWORD = WIFI_PASSWORD;
uint32_t PASSKEY = BLE_PASSKEY;

const unsigned int STUDENT_ID = 40250986;

const unsigned char BUZZER_PIN = 21;

volatile bool isDeviceConnected = false;
volatile bool isDeviceAuthenticated = false;

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
  MP_Stop,
  MP_Play
};

MP_States MP_State = MP_Start;

const unsigned char MAX_SONG_QUEUE_SIZE = 10;
vector<String> songQueue;
signed int currentSongIndex;
JsonDocument currentSongDoc;
JsonArray currentSongMelody;
unsigned char currentSongTempo;

unsigned short currentNoteIndex; // Index of the note (0, 2, 4, ...)
unsigned long noteCounter;       // Ticks (ms) remaining for current note/rest

volatile bool handlePlayPause = false;
volatile bool handleNextSong = false;
volatile bool handlePreviousSong = false;
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    isDeviceConnected = true;
    Serial.println("Device connected");
  };

  void onDisconnect(BLEServer *pServer)
  {
    isDeviceConnected = false;
    isDeviceAuthenticated = false;
    Serial.println("Device disconnected");
    // Restart advertising to allow new connections
    pServer->getAdvertising()->start();
    Serial.println("Restarting advertising...");
  }
};

class MySecurityCallbacks : public BLESecurityCallbacks
{
  // This is called when the pairing process is complete.
  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl)
  {
    if (auth_cmpl.success)
    {
      isDeviceAuthenticated = true;
      Serial.println("Pairing successful. Connection fully established.");
    }
    else
    {
      isDeviceAuthenticated = false;
      Serial.println("Authentication failed.");
    }
  }

  // The ESP32 will ask for a passkey to be entered on the client device.
  // This callback is not strictly needed for static passkey but is good practice.
  uint32_t onPassKeyRequest()
  {
    return PASSKEY;
  }

  // This is not used in the static passkey scenario but must be provided.
  void onPassKeyNotify(uint32_t pass_key) {}

  bool onConfirmPIN(uint32_t pass_key)
  {
    return true;
  }

  // This is not used in the static passkey scenario but must be provided.
  bool onConfirmRequest(uint32_t pass_key)
  {
    return true;
  }

  // This is not used in the static passkey scenario but must be provided.
  bool onSecurityRequest()
  {
    return true;
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    String command = value.c_str();

    if (value.length() > 0 && isDeviceAuthenticated)
    {
      switch (value[0])
      {
      // Play/Pause
      case '1':
        handlePlayPause = true;
        break;
      // Next Song
      case '2':
        handleNextSong = true;
        break;
      // Previous Song
      case '3':
        handlePreviousSong = true;
        break;
      default:
        Serial.println("-> Unknown command: " + command);
        break;
      }
    }
  }
};

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

void setup()
{
  pinMode(BUZZER_PIN, OUTPUT);

  // Wait for Serial to be ready
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  // Free Classic BT controller memory (we only use BLE)
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  // Create the BLE Device
  BLEDevice::init("ESP32_MusicPlayer_" + std::to_string(STUDENT_ID));

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Security Callbacks
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  // Set up security
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT; // Display a PIN
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &PASSKEY, sizeof(uint32_t));

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  // Add a descriptor to name the characteristic
  BLEDescriptor *pDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
  pDescriptor->setValue("Music Control");
  pDescriptor->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  pCharacteristic->addDescriptor(pDescriptor);

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("Advertising Started. Waiting for a client connection...");

  // Connect to Wi-Fi
  connectToWifi();

  // Initialize State Machine Timer ---
  // Specify Timer ID, prescaler 80 (for 1MHz, 1us ticks), count up.
  MB_Timer = timerBegin(MB_TIMER_ID, 80, true);
  // Attach the ISR to the timer
  timerAttachInterrupt(MB_Timer, &MB_TimerISR, true);
  // Set alarm to trigger every PERIOD, and auto-reload
  timerAlarmWrite(MB_Timer, MB_PERIOD, true);
  // Start the timer's alarm
  enableTimer(MB_Timer);

  // Serial.println("Heap Memory After Setup:");
  // Serial.printf("Free Heap: %u bytes, Max Contiguous Block: %u bytes\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

/**
 * @brief Fetches song data from the API and parses it into a JsonDocument.
 * @param songName The name of the song to fetch.
 * @param doc A reference to a JsonDocument to store the parsed JSON data.
 * @return true if the song was fetched and parsed successfully, false otherwise.
 */
bool getSongJsonByName(const String &songName, JsonDocument &doc)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected, cannot fetch song.");
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.useHTTP10(true);
  http.begin(secureClient, Api::UrlBuilder::getSongByName(songName));

  int httpResponseCode = http.GET();
  if (httpResponseCode != HTTP_CODE_OK)
  {
    String response = http.getString();
    if (response.length() > 120)
    {
      response = response.substring(0, 120) + "...";
    }
    Serial.printf("Error [%d]: %s\n", httpResponseCode, response.c_str());
    http.end();
    return false;
  }

  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  return true;
}

/**
 * @brief Fetches song data from the API and parses it into a JsonDocument.
 * @param doc A reference to a JsonDocument to store the parsed JSON data.
 * @return true if the song was fetched and parsed successfully, false otherwise.
 */
bool getSongJson(JsonDocument &doc)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected, cannot fetch song.");
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.useHTTP10(true);
  http.begin(secureClient, Api::UrlBuilder::getSong());

  int httpResponseCode = http.GET();
  if (httpResponseCode != HTTP_CODE_OK)
  {
    String response = http.getString();
    if (response.length() > 120)
    {
      response = response.substring(0, 120) + "...";
    }
    Serial.printf("Error [%d]: %s\n", httpResponseCode, response.c_str());
    http.end();
    return false;
  }

  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  return true;
}

void resetPlayback()
{
  currentNoteIndex = 0;
  noteCounter = 0;
  noTone(BUZZER_PIN);
}

/**
 * @brief Fetches a new random song, adds to queue, and loads it.
 * @return true on success, false on failure.
 */
bool fetchAndLoadSong()
{
  resetPlayback();

  JsonDocument doc;
  if (getSongJson(doc))
  {
    // Success
    songQueue.push_back(doc["name"].as<String>());
    currentSongIndex = songQueue.size() - 1; // It's the last one

    if (songQueue.size() > MAX_SONG_QUEUE_SIZE)
    {
      Serial.printf("Song queue full [Max: %d]. Removing oldest song.\n", MAX_SONG_QUEUE_SIZE);
      songQueue.erase(songQueue.begin()); // Remove the oldest song
      currentSongIndex--;                 // Adjust the current index since we removed one
    }

    // Load it into the global vars
    currentSongDoc = doc; // Copy the document
    currentSongMelody = currentSongDoc["melody"].as<JsonArray>();
    currentSongTempo = currentSongDoc["tempo"].as<unsigned char>();

    Serial.printf("Loaded new song: %s (Tempo: %d)\n", doc["name"].as<const char *>(), currentSongTempo);
  }
  else
  {
    // Failed
    Serial.println("Failed to fetch new song.");
    return false;
  }
  return true;
}

/**
 * @brief Loads a song from the queue based on currentSongIndex.
 * @return true on success, false on failure.
 */
bool loadSongFromQueue()
{
  resetPlayback();

  if (currentSongIndex < 0 || currentSongIndex >= songQueue.size())
  {
    Serial.println("Invalid song index.");
    return false;
  }

  String songName = songQueue[currentSongIndex];
  JsonDocument doc;
  if (getSongJsonByName(songName, doc))
  {
    // Success
    currentSongDoc = doc; // Copy
    currentSongMelody = currentSongDoc["melody"].as<JsonArray>();
    currentSongTempo = currentSongDoc["tempo"].as<unsigned char>();

    Serial.printf("Loaded from queue: %s (Tempo: %d)\n", doc["name"].as<const char *>(), currentSongTempo);
  }
  else
  {
    Serial.printf("Failed to fetch song '%s' from API.\n", songName.c_str());
    return false;
  }
  return true;
}

/**
 * @brief Synchronous State Machine Tick Function for Music Playback
 */
void TickFct_MusicPlayback()
{
  // --- 1. Handle Input Triggers (Transitions) ---
  // These flags are set by the BLE callback.
  // We consume them here in the main synchronous tick.

  if (handlePlayPause)
  {
    handlePlayPause = false;
    if (MP_State == MP_Play)
    {
      // Was playing, now pause
      MP_State = MP_Stop;
      noTone(BUZZER_PIN); // Stop current note
      Serial.println("⏸ Playback Paused.");
    }
    else if (MP_State == MP_Stop)
    {
      // Was stopped, now play
      if (currentSongIndex == -1)
      {
        // No song loaded. Need to fetch one.
        Serial.println("No song loaded. Fetching new one...");
        if (fetchAndLoadSong())
        {
          MP_State = MP_Play;
          Serial.printf("▶ Now playing: %s\n", currentSongDoc["name"].as<const char *>());
        }
        else
        {
          Serial.println("Failed to fetch song. Staying stopped.");
        }
      }
      else
      {
        // We have a song, just resume.
        MP_State = MP_Play;
        Serial.println("⏯ Playback Resumed.");
      }
    }
  }

  if (handleNextSong)
  {
    handleNextSong = false;
    currentSongIndex++; // Move to next
    if (currentSongIndex >= songQueue.size())
    {
      // End of queue, fetch a new one
      Serial.println("End of queue. Fetching new song...");
      if (fetchAndLoadSong())
      {
        MP_State = MP_Play; // Play it
        Serial.printf("▶ Now playing: %s\n", currentSongDoc["name"].as<const char *>());
      }
      else
      {
        Serial.println("Failed to fetch. Reverting to stop.");
        currentSongIndex--; // Revert index
        MP_State = MP_Stop;
      }
    }
    else
    {
      // Just load the next song from the queue
      Serial.println("⏭ Loading next song from queue...");
      if (loadSongFromQueue())
      {
        MP_State = MP_Play; // Play it
        Serial.printf("▶ Now playing: %s\n", currentSongDoc["name"].as<const char *>());
      }
      else
      {
        Serial.println("Failed to load. Reverting to stop.");
        MP_State = MP_Stop;
      }
    }
  }

  if (handlePreviousSong)
  {
    handlePreviousSong = false;
    if (currentSongIndex > 0)
    {
      // We have a previous song
      currentSongIndex--;
      Serial.println("⏮ Loading previous song from queue...");
      if (loadSongFromQueue())
      {
        MP_State = MP_Play; // Play it
        Serial.printf("▶ Now playing: %s\n", currentSongDoc["name"].as<const char *>());
      }
      else
      {
        Serial.println("Failed to load. Reverting to stop.");
        MP_State = MP_Stop;
      }
    }
    else
    {
      // No previous song, do nothing.
      Serial.println("No previous song in queue.");
      // We stay in the current state (Stop or Play)
    }
  }

  // --- 2. State Actions (What to *do* in each state) ---
  switch (MP_State)
  {
  case MP_Start:
    currentSongIndex = -1;
    songQueue.clear();
    resetPlayback();

    Serial.println("Attempting to fetch and play initial song...");
    if (fetchAndLoadSong())
    {
      MP_State = MP_Play;
      Serial.printf("▶ Now playing: %s\n", currentSongDoc["name"].as<const char *>());
    }
    else
    {
      // Failure (e.g., no WiFi), wait for user
      MP_State = MP_Stop;
      Serial.println("Failed to fetch initial song. Waiting for command.");
    }
    break;

  case MP_Stop:
    // We are "paused" or "stopped".
    // Do nothing. Wait for an input to transition to MP_Play.
    // noteCounter and currentNoteIndex are preserved to allow resume.
    break;

  case MP_Play:
    if (noteCounter > 0)
    {
      // We are in the middle of a note's duration (or rest).
      // Just decrement the counter and wait.
      noteCounter--;
    }
    else
    {
      // noteCounter is 0. Time to play the *next* note.

      // Check if we are at the end of the song
      if (currentNoteIndex >= currentSongMelody.size())
      {
        // Song finished.
        Serial.println("Song finished. Auto-playing next...");
        // Trigger the "next song" logic for the next tick
        handleNextSong = true;
        break; // Exit MP_Play state logic
      }

      // --- Play the current note ---
      // Get note frequency and duration value
      unsigned short note = currentSongMelody[currentNoteIndex].as<int>();
      int durationValue = currentSongMelody[currentNoteIndex + 1].as<int>();

      // Calculate total duration in ms (which is ticks, since tick=1ms)
      unsigned long wholeNote = (60000 * 4) / currentSongTempo;
      unsigned long noteDurationMs = 0;

      if (durationValue != 0)
      {
        noteDurationMs = wholeNote / abs(durationValue);
        if (durationValue < 0)
        {
          noteDurationMs = (noteDurationMs * 3) / 2; // Dotted note
        }
      }
      else
      {
        noteDurationMs = 0; // Should not happen, but safe
      }

      // Set the state machine's counter for this note's *total* duration
      noteCounter = noteDurationMs;

      // If it's a playable note (not a rest)
      if (note > 0)
      {
        // Use the non-blocking tone function.
        // It will play for 90% of the duration and stop itself.
        unsigned long playDuration = (noteDurationMs * 9) / 10;
        if (playDuration > 0)
        {
          tone(BUZZER_PIN, note, playDuration);
        }
      }
      // If it's a rest (note == 0), we do nothing.
      // The buzzer is already off. We just let the noteCounter
      // run down for the duration of the rest.

      // Advance to the *next* note index for the *next* iteration
      currentNoteIndex += 2;
    }
    break;
  }
}

void loop()
{
  // static unsigned long lastPrintMillis = 0;
  // unsigned long now = millis();
  // if (now - lastPrintMillis >= 1000)
  // {
  //   lastPrintMillis = now;
  //   Serial.printf("Free Heap: %u bytes, Max Contiguous Block: %u bytes\n",
  //                 esp_get_free_heap_size(),
  //                 heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
  // }

  if (WiFi.status() != WL_CONNECTED)
  {
    // Stop the timer during reconnection
    disableTimer(MB_Timer);
    noTone(BUZZER_PIN); // Stop music if WiFi drops
    MP_State = MP_Stop; // Go to stop state
    Serial.println("WiFi Disconnected, trying to reconnect...");
    connectToWifi();

    // Only restart the timer if reconnection was successful
    if (WiFi.status() == WL_CONNECTED)
    {
      enableTimer(MB_Timer);
    }
    return; // Skip the rest of the loop this iteration
  }

  // If the device is not authenticated, don't run the state machine
  // This check prevents BLE commands from being processed before pairing.
  if (!isDeviceAuthenticated && isDeviceConnected)
  {
    // We are connected, but not yet paired/authenticated.
    // We can clear flags to avoid a backlog of commands.
    handlePlayPause = false;
    handleNextSong = false;
    handlePreviousSong = false;
  }

  if (MB_TimerEnabled && isDeviceAuthenticated)
  {
    TickFct_MusicPlayback();
    while (!MB_Tick)
      ;
    MB_Tick = false;
  }
  else if (!MB_TimerEnabled && WiFi.status() == WL_CONNECTED)
  {
    // Ensure timer is running if WiFi is connected
    enableTimer(MB_Timer);
  }
}