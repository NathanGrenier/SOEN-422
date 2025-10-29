#include <Arduino.h>
#include <iostream>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "credentials.h"
#include "api_config.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

const unsigned int STUDENT_ID = 40250986;

const String BLE_NAME_PHONE1[] = {"Yuma1", "doom"};
const String BLE_NAME_PHONE2[] = {"Yuma2", "starwars"};
const String VALID_BLE_NAMES[] = {BLE_NAME_PHONE1[0], BLE_NAME_PHONE2[0]};
String currentBTAddress;
String currentDeviceName;

const unsigned char BUZZER_PIN = 21;

const int BLE_SCAN_DURATION = 5; // seconds
BLEScan *pBLEScan;
volatile bool isScanning = false;
volatile bool isProcessing = false;

bool postUserSongPreference(unsigned int studentId, const String &bluetoothAddress, const String &songName);

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    // Do not process if we are already handling a device
    if (isProcessing)
    {
      return;
    }

    String deviceAddress = advertisedDevice.getAddress().toString().c_str();
    String deviceName = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "N/A";
    // Serial.printf("Found Device: Address: %s, Name: %s, RSSI: %d\n",
    //               deviceAddress.c_str(),
    //               deviceName.c_str(),
    //               advertisedDevice.getRSSI());

    bool isValidDevice = false;
    for (const String &validName : VALID_BLE_NAMES)
    {
      if (deviceName.equals(validName))
      {
        isValidDevice = true;
        break;
      }
    }

    if (isValidDevice)
    {
      currentBTAddress = deviceAddress;
      currentDeviceName = deviceName;
      isProcessing = true;
    }
  }
};

/**
 * @brief Callback for when a finite BLE scan is complete.
 * @param scanResults The results object containing devices found.
 */
void onScanComplete(BLEScanResults scanResults)
{
  Serial.println("Scan finished!");

  pBLEScan->clearResults();

  isScanning = false;
}

void connectToWifi()
{
  Serial.printf("Connecting to %s ", ssid);
  if (strcmp(ssid, "SOEN422") == 0)
  {
    IPAddress local_IP(172, 30, 140, 210);
    IPAddress gateway(172, 30, 140, 129);
    IPAddress subnet(255, 255, 255, 128);
    IPAddress primaryDNS(8, 8, 8, 8);
    IPAddress secondaryDNS(8, 8, 4, 4);
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
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

  BLEDevice::init("ESP32_40250986");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // active scan uses more power, but gets results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // Connect to Wi-Fi
  connectToWifi();

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi connection failed.");
  }

  // Start BLE Scan
  Serial.printf("Starting BLE scan for %d seconds...\n", BLE_SCAN_DURATION);
  pBLEScan->start(BLE_SCAN_DURATION, onScanComplete, false);
  isScanning = true;
}

/**
 * @brief Fetches song data from the API and parses it into a JsonDocument.
 * @param songName The name of the song to fetch.
 * @param doc A reference to a JsonDocument to store the parsed JSON data.
 * @return true if the song was fetched and parsed successfully, false otherwise.
 */
bool getSongJson(const String &songName, JsonDocument &doc)
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
  http.begin(secureClient, Api::UrlBuilder::getSong(songName));

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
 * @brief Fetches the user's song preference from the API.
 * @param studentId The student's ID.
 * @param bluetoothAddress The Bluetooth address of the device.
 * @param doc A reference to a JsonDocument to store the parsed JSON data.
 * @return true if the preference was fetched and parsed successfully, false otherwise.
 */
bool getUserSongPreference(unsigned int studentId, const String &bluetoothAddress, JsonDocument &doc)
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
  http.begin(secureClient, Api::UrlBuilder::getPreference(studentId, bluetoothAddress));

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
 * @brief Posts the user's song preference to the API.
 * @param studentId The student's ID.
 * @param bluetoothAddress The Bluetooth address of the device.
 * @param songName The name of the song to set as preference.
 * @return true if the preference was posted successfully, false otherwise.
 */
bool postUserSongPreference(unsigned int studentId, const String &bluetoothAddress, const String &songName)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected, cannot post song preference.");
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.begin(secureClient, Api::UrlBuilder::postPreference(studentId, bluetoothAddress, songName));

  // No body needed for this POST request
  int httpResponseCode = http.POST("");
  if (httpResponseCode != HTTP_CODE_OK && httpResponseCode != HTTP_CODE_CREATED)
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

  if (httpResponseCode != HTTP_CODE_OK && httpResponseCode != HTTP_CODE_CREATED)
  {
    Serial.printf("Error [%d]\n", httpResponseCode);
    http.end();
    return false;
  }
  http.end();

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

String songName = "";

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected, trying to reconnect...");
    connectToWifi();
  }

  // If we are not busy processing a device and the scan is not running, start it.
  if (!isProcessing && !isScanning)
  {
    Serial.printf("Starting BLE scan for %d seconds...\n", BLE_SCAN_DURATION);
    pBLEScan->start(BLE_SCAN_DURATION, onScanComplete, false);
    isScanning = true;
  }

  if (isProcessing)
  {
    JsonDocument preferenceDoc;
    JsonDocument songDoc;

    String songToSet = "";
    if (currentDeviceName.equalsIgnoreCase(BLE_NAME_PHONE1[0]))
    {
      songToSet = BLE_NAME_PHONE1[1];
    }
    else if (currentDeviceName.equalsIgnoreCase(BLE_NAME_PHONE2[0]))
    {
      songToSet = BLE_NAME_PHONE2[1];
    }

    Serial.println("Setting song preference to '" + songToSet + "' for device " + currentBTAddress);
    if (postUserSongPreference(STUDENT_ID, currentBTAddress, songToSet))
    {
      Serial.println("Successfully set song preference.");
      // Now fetch and play the song
      Serial.println("Fetching user song preference...");
      if (getUserSongPreference(STUDENT_ID, currentBTAddress, preferenceDoc))
      {
        Serial.println("Successfully fetched user song preference: ");
        songName = preferenceDoc["name"].as<String>();
      }
      else
      {
        Serial.println("Failed to retrieve user song preference.");
        songName = "";
      }

      if (songName != "")
      {
        Serial.println("Fetching user's preferred song data...");
        if (getSongJson(songName, songDoc))
        {
          Serial.println("Successfully fetched song. ");

          Serial.println("Playing song: " + songName);
          JsonArray melody = songDoc["melody"].as<JsonArray>();
          unsigned char tempo = songDoc["tempo"].as<unsigned char>();
          playSong(melody, tempo);
        }
        else
        {
          Serial.println("Failed to retrieve song data.");
        }
      }
      else
      {
        Serial.println("No song preference set for this device.");
      }
    }
    else
    {
      Serial.println("Failed to set song preference.");
    }

    // Reset state to allow for a new scan, regardless of success or failure
    Serial.println("Processing complete. Resetting for next scan.");
    currentBTAddress = "";
    currentDeviceName = "";
    songName = "";
    isProcessing = false;
  }
}