#include <Arduino.h>
#include <iostream>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "credentials.h"
#include "api_config.h"

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
        Serial.println("-> Play/Pause command received.");
        break;
      // Next Song
      case '2':
        Serial.println("-> Next Song command received.");
        break;
      // Previous Song
      case '3':
        Serial.println("-> Previous Song command received.");
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

JsonDocument songDoc;
JsonDocument preferenceDoc;
String songName = "";

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected, trying to reconnect...");
    connectToWifi();
  }
}