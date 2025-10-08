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
unsigned const char MB_PERIOD = 1000; // microseconds
volatile bool MB_Tick = false;
bool MB_TimerEnabled = false;

void IRAM_ATTR MB_TimerISR()
{
  MB_Tick = true;
}

enum MP_States
{
  MP_Start,
  MP_ClearSong,
  MP_Process,
  MP_FetchNextSong,
  MP_FetchSongData,
  MP_PrepareNote,
  MP_PlayNote,
  MP_SongEnd,
  MP_PlayPause,
  MP_NextSong,
  MP_PreviousSong,
};

MP_States MP_State = MP_Start;
MP_States MP_PrevState;

vector<String> songQueue;
signed int currentSongIndex;
JsonDocument currentSongDoc;
String currentSongName;
JsonArray currentSongMelody;
unsigned char currentSongTempo;
volatile bool isPlaying;

unsigned short currentNoteIndex;
unsigned long nextNoteTime;
unsigned short totalNoteTicks;
unsigned short noteCounter;

volatile bool handlePlayPause = false;
volatile bool handleNextSong = false;
volatile bool handlePreviousSong = false;

void handleClientInput()
{
  if (handlePlayPause)
  {
    handlePlayPause = false;
    if (isPlaying)
    {
      MP_PrevState = MP_State;
      MP_State = MP_PlayPause;
      noTone(BUZZER_PIN);
      isPlaying = false;
      Serial.println("-> Music Paused.");
    }
    else
    {
      isPlaying = true;
    }
  }

  if (handleNextSong)
  {
    handleNextSong = false;
  }

  if (handlePreviousSong)
  {
    handlePreviousSong = false;
  }
}

void TickFct_MusicPlayback()
{
  handleClientInput();

  switch (MP_State)
  { // Transitions
  case MP_Start:
    MP_State = MP_ClearSong;
    break;
  case MP_ClearSong:
    MP_State = MP_Process;
    break;
  case MP_Process:
    if (currentSongIndex < 0 || currentSongIndex >= songQueue.size())
    {
      MP_State = MP_FetchNextSong;
    }
    else if (currentSongDoc.isNull())
    {
      MP_State = MP_FetchSongData;
    }
    else if (currentSongName == currentSongDoc["name"].as<String>())
    {
      MP_State = MP_PrepareNote;
    }
    else
    {
      MP_State = MP_FetchSongData;
    }
    break;
  case MP_FetchNextSong:
    if (currentSongDoc.isNull())
    {
      MP_State = MP_FetchNextSong;
    }
    else if (!currentSongDoc.isNull())
    {
      MP_State = MP_PrepareNote;
    }
    break;
  case MP_FetchSongData:
    if (currentSongDoc.isNull())
    {
      MP_State = MP_FetchSongData;
    }
    else if (!currentSongDoc.isNull())
    {
      MP_State = MP_PrepareNote;
    }
    break;
  case MP_PrepareNote:
    MP_State = MP_PlayNote;
    noteCounter = 0;
    break;
  case MP_PlayNote:
    if (noteCounter <= totalNoteTicks)
    {
      MP_State = MP_PlayNote;
      noteCounter++;
    }
    else if (noteCounter > totalNoteTicks)
    {
      if (currentNoteIndex > currentSongMelody.size() / 2)
      {
        MP_State = MP_SongEnd;
      }
      else
      {
        currentNoteIndex += 2;
        MP_State = MP_PrepareNote;
      }
    }
    break;
  case MP_SongEnd:
    MP_State = MP_ClearSong;
    break;
  case MP_PlayPause:
    if (isPlaying)
    {
      MP_State = MP_PrevState;
      Serial.println("-> Music Resumed.");
    }
    else
    {
      MP_State = MP_PlayPause;
    }
    break;
  case MP_NextSong:
    break;
  case MP_PreviousSong:
    break;
  default:
    MP_State = MP_ClearSong;
  }

  switch (MP_State)
  { // State actions
  case MP_Start:
    currentSongIndex = -1;
    break;
  case MP_ClearSong:
    currentSongDoc.clear();
    currentSongName = "";
    break;
  case MP_Process:
    if (currentSongIndex > 0 && currentSongIndex <= songQueue.size())
    {
      currentSongName = songQueue[currentSongIndex];
    }
    break;
  case MP_FetchNextSong:
    // Fetch next song.
    // Store the name and song data.
    break;
  case MP_FetchSongData:
    // Fetch Song Data using name of song.
    break;
  case MP_PlayNote:
    if (noteCounter == 0)
    {
      playNote();
    }
    break;
  case MP_SongEnd:
    currentSongIndex++;
    currentNoteIndex = 0;
    nextNoteTime = 0;
    isPlaying = false;
    break;
  case MP_PrepareNote:
    noteCounter = 0;
    wholeNote = (60000 * 4) / tempo;
    durationValue = melody[currentNoteIndex + 1];
    nextNoteTime = wholeNote / abs(durationValue);

    if (durationValue < 0)
    {
      nextNoteTime *= 1.5;
    }

    totalNoteTicks = nextNoteTime / PERIOD;
    break;
  case MP_PlayPause:
    break;
  case MP_NextSong:
    break;
  case MP_PreviousSong:
    break;
  default: // ADD default behaviour below
    break;
  }
}

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

    if (value.length() > 0)
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
  // Speicfy Timer ID, prescaler 80 (for 1MHz, 1us ticks), count up.
  MB_Timer = timerBegin(MB_TIMER_ID, 80, true);
  // Attach the ISR to the timer
  timerAttachInterrupt(MB_Timer, &MB_TimerISR, true);
  // Set alarm to trigger every PERIOD, and auto-reload
  timerAlarmWrite(MB_Timer, MB_PERIOD, true);
  // Start the timer's alarm
  enableTimer(MB_Timer);
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

/**
 * @brief Plays the entire melody on the buzzer.
 * @param melody A JsonArray containing pairs of note frequencies and durations.
 * @param tempo The tempo of the song in beats per minute (BPM).
 */
void playSong(const JsonArray &melody, unsigned char tempo)
{
  // Note duration math taken from: https://github.com/robsoncouto/arduino-songs/blob/master/happybirthday/happybirthday.ino
  unsigned int wholeNote = (60000 * 4) / tempo;

  for (int i = 0; i < melody.size(); i += 2)
  {
    // Get the note frequency and duration from the array
    unsigned short note = melody[i].as<int>();
    char durationValue = melody[i + 1].as<int>();

    // Avoid division by zero
    if (durationValue == 0)
      continue;

    // Calculate the duration of the note
    unsigned int noteDuration = wholeNote / abs(durationValue);

    // If the duration is negative, it's a dotted note (1.5x duration)
    if (durationValue < 0)
    {
      noteDuration *= 1.5;
    }

    // A note of 0 is a rest, so just delay for the duration
    if (note > 0)
    {
      tone(BUZZER_PIN, note);
      delay(noteDuration * 0.9); // Play for 90% of duration
      noTone(BUZZER_PIN);

      // Pause for the remaining 10% to create a gap between notes
      delay(noteDuration * 0.1);

      // tone(BUZZER_PIN, note, noteDuration * 0.9); // Asynchronous tone playback (stops after duration)
    }
    else
    {
      delay(noteDuration); // Rest
    }
  }
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    // Stop the timer during reconnection
    disableTimer(MB_Timer);
    Serial.println("WiFi Disconnected, trying to reconnect...");
    connectToWifi();

    // Only restart the timer if reconnection was successful
    if (WiFi.status() == WL_CONNECTED)
    {
      enableTimer(MB_Timer);
    }
    return; // Skip the rest of the loop this iteration
  }

  if (MB_TimerEnabled)
  {
    TickFct_MusicPlayback();
    while (!MB_Tick)
      ;
    MB_Tick = false;
  }
}