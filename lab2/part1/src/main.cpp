#include <Arduino.h>
#include <iostream>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "credentials.h"
#include "api_config.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

const unsigned int STUDENT_ID = 40250986;

const String BT_ADDRESS_PHONE = "00:2b:70:aa:3e:92";
const String BT_ADDRESS_LAPTOP = "E0:0A:F6:B8:D5:76";
const String VALID_BT_ADDRESS[] = {BT_ADDRESS_PHONE, BT_ADDRESS_LAPTOP};
String currentBTAddress;

const unsigned char BUZZER_PIN = 21;

BluetoothSerial SerialBT;
bool isScanning;

bool postUserSongPreference(unsigned int studentId, const String &bluetoothAddress, const String &songName);

void btAdvertisedDeviceFound(BTAdvertisedDevice *pDevice)
{
  Serial.printf("Found a device asynchronously: %s\n", pDevice->toString().c_str());

  bool isValidDevice = false;
  String deviceAddress = pDevice->getAddress().toString();

  for (const String &validAddress : VALID_BT_ADDRESS)
  {
    if (deviceAddress.equals(validAddress))
    {
      isValidDevice = true;
      break;
    }
  }

  if (isValidDevice)
  {
    currentBTAddress = deviceAddress;
    Serial.println("Device Match Found for: " + deviceAddress);
    SerialBT.discoverAsyncStop();
    isScanning = false;
  }
  else
  {
    Serial.println("Unknown device: " + deviceAddress + " - Ignoring.");
    return;
  }
}

void setSongPreferenceOnStartup()
{
  postUserSongPreference(STUDENT_ID, BT_ADDRESS_PHONE, "harrypotter");
  postUserSongPreference(STUDENT_ID, BT_ADDRESS_LAPTOP, "doom");
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

  // // Bluetooth Serial
  SerialBT.begin("ESP32_40250986"); // Bluetooth device name

  Serial.print("Starting asynchronous discovery... ");
  if (SerialBT.discoverAsync(btAdvertisedDeviceFound))
  {
    isScanning = true;
  }
  else
  {
    Serial.println("Error starting discoverAsync");
    isScanning = false;
  }

  // Connect to Wi-Fi
  connectToWifi();

  if (WiFi.status() == WL_CONNECTED)
  {
    // setSongPreferenceOnStartup();
  }
  else
  {
    Serial.println("WiFi connection failed.");
  }
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

  HTTPClient http;
  String url = Api::UrlBuilder::getSong(songName);
  http.begin(url);

  int httpResponseCode = http.GET();
  if (httpResponseCode <= 0)
  {
    Serial.printf("Error on HTTP request: %d\n", httpResponseCode);
    http.end();
    return false;
  }

  http.end();

  DeserializationError error = deserializeJson(doc, http.getStream());
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

  HTTPClient http;
  http.begin(Api::UrlBuilder::getPreference(studentId, bluetoothAddress));

  int httpResponseCode = http.GET();
  if (httpResponseCode <= 0)
  {
    Serial.printf("Error on HTTP request: %d\n", httpResponseCode);
    http.end();
    return false;
  }

  http.end();

  DeserializationError error = deserializeJson(doc, http.getStream());
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

  HTTPClient http;
  String url = Api::UrlBuilder::postPreference(studentId, bluetoothAddress, songName);
  http.begin(url);

  // No body needed for this POST request
  int httpResponseCode = http.POST("");
  if (httpResponseCode <= 0)
  {
    Serial.printf("Error on HTTP POST request: %d\n", httpResponseCode);
    http.end();
    return false;
  }

  String response = http.getString();

  if (httpResponseCode == HTTP_CODE_OK && response.length() > 0)
  {
    Serial.println("Response from server: " + response);
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
      tone(BUZZER_PIN, note, noteDuration * 0.9); // Play for 90% of duration
    }

    // Pause for the full duration to separate notes
    delay(noteDuration);

    // Stop the waveform generation before the next note
    noTone(BUZZER_PIN);
  }
}

const String VALID_SONGS[] = {"harrypotter", "doom"};

JsonDocument songDoc;
JsonDocument preferenceDoc;
String songName = "";

void loop()
{
  unsigned long now = millis();

  if (currentBTAddress == "" && !isScanning)
  {
    Serial.println("No valid device connected, restarting discovery...");
    if (SerialBT.discoverAsync(btAdvertisedDeviceFound))
    {
      isScanning = true;
    }
    else
    {
      Serial.println("Error starting discoverAsync");
    }
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected, trying to reconnect...");
    connectToWifi();
  }

  if (currentBTAddress != "")
  {
    if (preferenceDoc.isNull())
    {
      Serial.println("Fetching user song preference...");
      if (getUserSongPreference(STUDENT_ID, currentBTAddress, preferenceDoc))
      {
        Serial.println("Successfully fetched user song preference: ");
        Serial.println("JSON Preference Data: ");
        serializeJsonPretty(preferenceDoc, Serial);
        Serial.println();

        songName = preferenceDoc["name"].as<String>();
      }
      else
      {
        Serial.println("Failed to retrieve user song preference.");
      }
    }

    if (songDoc.isNull())
    {
      if (songName != "")
      {
        Serial.println("Fetching user's preferred song data...");
        if (getSongJson(songName, songDoc))
        {
          Serial.println("Successfully fetched song: ");
          Serial.println("JSON Song Data: ");
          serializeJsonPretty(songDoc, Serial);
          Serial.println();
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

    if (!songDoc.isNull())
    {
      Serial.println("Playing song: " + songName);
      JsonArray melody = songDoc["melody"].as<JsonArray>();
      unsigned char tempo = songDoc["tempo"].as<unsigned char>();
      playSong(melody, tempo);

      preferenceDoc.clear();
      songDoc.clear();
      songName = "";
      currentBTAddress = "";
    }
  }
}
