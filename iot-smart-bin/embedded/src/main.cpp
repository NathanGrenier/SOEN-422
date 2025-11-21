#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HCSR04.h>
#include "api_config.h"
#if defined(PRODUCTION_BUILD)
#include <WebSocketsClient.h> // For WSS
#else
#include <WiFiClient.h> // For standard MQTT
#endif
#include <MQTTPubSubClient.h>

#ifdef CI_BUILD
#define WIFI_SSID "DUMMY_SSID"
#define WIFI_PASSWORD "DUMMY_PASSWORD"
#define BIN_ID "CI_BIN"
#define BIN_HEIGHT 100

#define MQTT_BROKER_URL "ci.dummy.prod"
#define MQTT_BROKER_PORT 443
#define MQTT_USERNAME "ci_user"
#define MQTT_PASSWORD "ci_pass"
#else
#if defined(PRODUCTION_BUILD)
#include "credentials.prod.h"
#else
#include "credentials.h"
#endif
#endif

// --- WiFi Credentials ---
const char *SSID = WIFI_SSID;
const char *PASSWORD = WIFI_PASSWORD;

const char *DEVICE_ID = BIN_ID;
String clientId;

// --- Sensor Setup ---
const byte ECHO_PIN = 16;          // White Wire
const byte TRIGGER_PIN = 17;       // Yellow Wire
const u16_t MAX_DISTANCE_CM = 400; // Maximum distance to measure (in cm)
UltraSonicDistanceSensor distanceSensor = UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE_CM);

// --- Bin Configuration ---
const float BIN_HEIGHT_CM = BIN_HEIGHT; // The total depth of the bin
const float MIN_VALID_CM = 2.0;         // Sensor blind spot

// --- Timing & State Machine Variables ---
unsigned long lastCycleTime = 0;
const long CYCLE_INTERVAL = 15 * 1000; // Time between sampling bursts in milliseconds

// Sampling State Machine
bool isSampling = false;
unsigned long lastSampleTime = 0; // Tracks the 60ms gap
const long SAMPLE_INTERVAL = 60;  // Time between individual pings
const int TARGET_SAMPLES = 11;
std::vector<float> currentReadings;

// --- MQTT Client Setup ---
#if defined(PRODUCTION_BUILD)
WebSocketsClient client; // For WSS (Secure WebSocket)
const char *ENV_SUFFIX = "-prod";
#else
WiFiClient client; // For standard unsecured MQTT
const char *ENV_SUFFIX = "-dev";
#endif

const char *BROKER_URL = MQTT_BROKER_URL;
const uint16_t BROKER_PORT = MQTT_BROKER_PORT;
const char *MQTT_USER = MQTT_USERNAME;
const char *MQTT_PASS = MQTT_PASSWORD;

MQTTPubSubClient mqtt;

// --- Forward Declarations ---
void triggerSampling();

void enterDeepSleep(uint64_t time_ms)
{
  Serial.println("Going to sleep to save battery...");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(time_ms * 1000);
  esp_deep_sleep_start();
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
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - startTime > 10000)
    {
      Serial.println("\nWiFi Timeout! Router might be down.");
      enterDeepSleep(5 * 60 * 1000);
    }
    delay(500);
    Serial.print(".");
  }
  // This enables Modem Sleep. The radio turns off between DTIM intervals.
  WiFi.setSleep(true);

  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setupMqttSubscriptions()
{
  mqtt.subscribe(MQTT::Topics::getPing(DEVICE_ID), [](const String &payload, const size_t size)
                 {
        Serial.println("!!! FORCING SAMPLE !!!");
        triggerSampling(); });
}

void connectToMqtt()
{
  Serial.print("Connecting to MQTT broker... ");

#if defined(PRODUCTION_BUILD)
  // WebSocketsClient handles reconnection internally in its loop,
#else
  if (!client.connected())
  {
    Serial.print("Re-establishing TCP... ");
    client.stop();
    if (!client.connect(BROKER_URL, BROKER_PORT))
    {
      Serial.println("TCP Failed. Retrying later.");
      delay(2000);
      return;
    }
    Serial.println("TCP OK.");
  }
#endif

  String statusTopic = MQTT::Topics::getStatus(DEVICE_ID);

  // Set the Last Will before connecting. If mqtt connection is lost, it will publish "offline" to this topic
  mqtt.setWill(statusTopic, "offline", true, 0);

  if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS))
  {
    Serial.println(" MQTT Connected!");
    setupMqttSubscriptions();
    mqtt.publish(statusTopic, "online", true, 0); // Announce we are Online immediately
  }
  else
  {
    Serial.print(" MQTT Failed (Error: ");
    Serial.print(mqtt.getLastError());
    Serial.println(")");
#if !defined(PRODUCTION_BUILD)
    client.stop(); // force close TCP to start fresh next time
#endif
    delay(2000);
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  clientId = String(DEVICE_ID) + String(ENV_SUFFIX);
  Serial.printf("Device Configured: %s\n", clientId.c_str());

  connectToWifi();

// --- Configure the correct client ---
#if defined(PRODUCTION_BUILD)
  Serial.println("Mode: PRODUCTION (WSS)");
  client.beginSSL(BROKER_URL, BROKER_PORT, "/", "", "mqtt");
  client.setReconnectInterval(3000);
#else
  Serial.println("Mode: DEVELOPMENT (unsecured MQTT)");
  client.setTimeout(5000);
  if (!client.connect(BROKER_URL, BROKER_PORT))
  {
    Serial.println("Initial TCP failed.");
  }
#endif

  mqtt.begin(client);

  // mqtt.subscribe([](const String &topic, const String &payload, size_t size)
  //                { Serial.printf("[Global] Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str()); });

  while (!mqtt.isConnected())
  {
    connectToMqtt();
    if (!mqtt.isConnected())
      delay(3000);
  }

  Serial.println("Heap Memory After Setup:");
  Serial.printf("Free Heap: %u bytes, Max Contiguous Block: %u bytes\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

/**
 * Takes the collected vector, sorts it, trims outliers, and returns average.
 */
float processReadings(std::vector<float> &readings)
{
  if (readings.empty())
    return -1.0;
  if (readings.size() < 3)
    return readings[0];

  // Sort to isolate outliers
  std::sort(readings.begin(), readings.end());

  // Trim bottom 25% and top 25%
  int trimCount = readings.size() / 4;

  float sum = 0;
  int count = 0;

  for (int i = trimCount; i < readings.size() - trimCount; i++)
  {
    sum += readings[i];
    count++;
  }

  if (count == 0)
    return -1.0;
  return sum / count;
}

void triggerSampling()
{
  if (!isSampling)
  {
    isSampling = true;
    lastCycleTime = millis();
    lastSampleTime = millis() - SAMPLE_INTERVAL;
    currentReadings.clear();
    Serial.println("Starting sampling burst...");
  }
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectToWifi();
  }

  if (!mqtt.isConnected())
  {
    Serial.println("MQTT disconnected. Reconnecting...");
    connectToMqtt();
  }

  mqtt.update();

  unsigned long now = millis();

  if (!isSampling && (now - lastCycleTime > CYCLE_INTERVAL))
  {
    triggerSampling();
  }

  if (isSampling)
  {
    if (now - lastSampleTime >= SAMPLE_INTERVAL)
    {
      lastSampleTime = now;

      // Note: measureDistanceCm() blocks for ~20ms max (hardware limitation of pulseIn)
      float val = distanceSensor.measureDistanceCm();

      if (val > MIN_VALID_CM && val <= BIN_HEIGHT_CM)
      {
        currentReadings.push_back(val);
      }

      // Check if we have collected enough samples (attempted TARGET_SAMPLES times)
      static int attempts = 0;
      attempts++;

      if (attempts >= TARGET_SAMPLES)
      {
        // Burst Complete
        isSampling = false;
        attempts = 0;

        Serial.printf("Collected %d valid samples.\n", currentReadings.size());

        float distance = processReadings(currentReadings);

        if (distance > 0)
        {
          int fillPercentage = map((long)distance, 0, (long)BIN_HEIGHT_CM, 100, 0);
          fillPercentage = constrain(fillPercentage, 0, 100);

          Serial.printf("Result: %.2f cm, Fill: %d%%\n", distance, fillPercentage);

          JsonDocument doc;
          doc["deviceId"] = DEVICE_ID;
          doc["fillLevel"] = fillPercentage;

          char output[256];
          serializeJson(doc, output);

          mqtt.publish(MQTT::Topics::getData(DEVICE_ID), output);
          Serial.print("Published to ");
          Serial.print(MQTT::Topics::getData(DEVICE_ID));
          Serial.print(": ");
          Serial.println(output);
        }
        else
        {
          Serial.println("Error: No valid readings in this burst.");
        }

        // Serial.printf("Free Heap: %u bytes, Max Contiguous Block: %u bytes\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
      }
    }
  }

  delay(10); // Trigger Modem Sleep
}