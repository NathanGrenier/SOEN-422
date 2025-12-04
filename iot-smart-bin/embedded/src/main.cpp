#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <HCSR04.h>
#include <TiltSensor.h>
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
#define BATTERY_VOLTAGE_CALIBRATION 0.0

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

const byte TILT_PIN = 13; // White Wire
// RBS 040100 is Closed when Upright, so we set normallyClosed = true
TiltSensor tiltSensor(TILT_PIN, true);
unsigned long tiltDebounceStartTime = 0;
const unsigned long TILT_DEBOUNCE_DELAY = 200;

// --- Servo Configuration ---
const byte RED_LED_PIN = 14;    // Blue Wire
const byte SERVO_DATA_PIN = 12; // Yellow Wire
const byte SERVO_TIMER_ID = 0;
const byte SERVO_PERIOD = 50; // in Hz
// Datasheet for Hitec HS-422: https://media.digikey.com/pdf/data%20sheets/dfrobot%20pdfs/ser0002_web.pdf
const u16_t SERVO_MIN = 900;
const u16_t SERVO_MAX = 2100;

const u16_t SERVO_BIN_CLOSED_POS = 0; // Degrees
const u16_t SERVO_BIN_OPEN_POS = 90;  // Degrees

Servo servo;

// --- Bin Configuration ---
int threshold = 85;                     // Default value, overwritten by MQTT
const float BIN_HEIGHT_CM = BIN_HEIGHT; // The total depth of the bin
const float MIN_VALID_CM = 2.0;         // Sensor blind spot
int lastValidFillLevel = 0;

// --- Battery Configuration ---
const int BATTERY_PIN = 35;
const float VOLTAGE_CALIBRATION = BATTERY_VOLTAGE_CALIBRATION;
// --- Timing & State Machine Variables ---
unsigned long lastCycleTime = 0;
const long CYCLE_INTERVAL = 10 * 1000; // Time between sampling bursts in milliseconds

// Sampling State Machine
bool isSampling = false;
unsigned long lastSampleTime = 0; // Tracks the 60ms gap
const long SAMPLE_INTERVAL = 60;  // Time between individual pings
const int TARGET_SAMPLES = 11;
std::vector<float> currentReadings;

// Tilt Recovery Logic
bool wasTilted = false;

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
void openBin();

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

  mqtt.subscribe(MQTT::Topics::getConfig(DEVICE_ID), [](const String &payload, const size_t size)
                 {
    Serial.printf("Received config update: %s\n", payload.c_str());
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
        if (doc["threshold"].is<int>()) {
            threshold = doc["threshold"];
            Serial.printf("New Threshold Set: %d%%\n", threshold);
            // Trigger a sample immediately to apply new threshold logic
            triggerSampling();
        }
    } else {
        Serial.println("Failed to parse config JSON");
    } });
}

void requestThreshold()
{
  Serial.println("Requesting threshold from server...");
  mqtt.publish(MQTT::Topics::requestConfig(DEVICE_ID), "{}");
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
    requestThreshold();
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

  analogReadResolution(12);

  // --- LED Setup ---
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW); // Start Off

  // --- Servo Setup ---
  ESP32PWM::allocateTimer(SERVO_TIMER_ID);
  servo.setPeriodHertz(SERVO_PERIOD);
  servo.attach(SERVO_DATA_PIN, SERVO_MIN, SERVO_MAX);

  // --- Tilt Sensor Setup ---
  tiltSensor.begin();

  // --- Battery Pin Setup ---
  pinMode(BATTERY_PIN, INPUT);

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

  // Ensure bin is opened at startup by default
  openBin();

  Serial.println("Heap Memory After Setup:");
  Serial.printf("Free Heap: %u bytes, Max Contiguous Block: %u bytes\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

void openBin()
{
  Serial.println("[ACTUATOR] Opening bin (Under Threshold)");
  digitalWrite(RED_LED_PIN, LOW);
  servo.write(SERVO_BIN_OPEN_POS);
}

void closeBin()
{
  Serial.println("[ACTUATOR] Closing bin (Over Threshold)");
  digitalWrite(RED_LED_PIN, HIGH);
  servo.write(SERVO_BIN_CLOSED_POS);
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

struct BatteryMap
{
  float voltage;
  int percentage;
};

// Data points derived specifically from Samsung 25R Datasheet (Page 6, 1C Curve)
// Samsung INR18650-25R Datasheet: https://www.powerstream.com/p/INR18650-25R-datasheet.pdf
const BatteryMap BATTERY_CURVE[] = {
    {4.20, 100},
    {3.88, 80},
    {3.70, 60},
    {3.60, 50},
    {3.53, 40},
    {3.4, 20},
    {3.27, 10},
    {3.10, 0}};
const int TABLE_SIZE = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);

/**
 * Calculates a precise battery percentage using Linear Interpolation
 * based on the Samsung 25R datasheet.
 */
int getBatteryPercentage(float voltage)
{
  // Handle edge cases (Overcharged or Empty)
  if (voltage >= BATTERY_CURVE[0].voltage)
    return 100;
  if (voltage <= BATTERY_CURVE[TABLE_SIZE - 1].voltage)
    return 0;

  for (int i = 0; i < TABLE_SIZE - 1; i++)
  {
    float upperV = BATTERY_CURVE[i].voltage;
    float lowerV = BATTERY_CURVE[i + 1].voltage;

    if (voltage <= upperV && voltage > lowerV)
    {
      int upperP = BATTERY_CURVE[i].percentage;
      int lowerP = BATTERY_CURVE[i + 1].percentage;

      float progress = (voltage - lowerV) / (upperV - lowerV);

      return lowerP + (progress * (upperP - lowerP));
    }
  }

  // Should not reach here
  return 0;
}

/**
 * Reads the analog pin and converts to battery voltage
 */
float readBatteryVoltage()
{
  uint32_t raw = analogRead(BATTERY_PIN);

  // Formula: (ADC / Max_ADC) * Ref_Voltage * Divider_Factor
  // The divider cuts voltage in half, so we multiply by 2.
  float voltage = (raw / 4095.0) * 3.3 * 2.0;

  return voltage + VOLTAGE_CALIBRATION;
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

  // --- TILT LOGIC ---
  if (tiltSensor.isTilted())
  {
    if (tiltDebounceStartTime == 0)
    {
      tiltDebounceStartTime = now;
    }

    else if (now - tiltDebounceStartTime > TILT_DEBOUNCE_DELAY)
    {
      if (!wasTilted)
      {
        Serial.println("[TILT] Bin tilted (Confirmed)! Pausing sampling.");
        wasTilted = true;
        isSampling = false;

        float voltage = readBatteryVoltage();
        int batteryLevel = getBatteryPercentage(voltage);

        JsonDocument doc;
        doc["deviceId"] = DEVICE_ID;
        doc["fillLevel"] = lastValidFillLevel;
        doc["batteryPercentage"] = batteryLevel;
        doc["voltage"] = round(voltage * 100.0) / 100.0;
        doc["isTilted"] = true;

        char output[256];
        serializeJson(doc, output);
        mqtt.publish(MQTT::Topics::getData(DEVICE_ID), output);
        Serial.print("Published Tilt Alert: ");
        Serial.println(output);
      }

      // While effectively tilted, we reset the loop to avoid sampling
      delay(100);
      return;
    }
  }
  else
  {
    tiltDebounceStartTime = 0;
  }

  // --- RECOVERY LOGIC ---
  if (wasTilted && !tiltSensor.isTilted())
  {
    Serial.println("[TILT] Bin upright. Triggering fast recovery sample in 2s.");
    wasTilted = false;
    lastCycleTime = now - CYCLE_INTERVAL + (2 * 1000);
  }

  // --- SAMPLING LOGIC ---
  if (!isSampling && (now - lastCycleTime > CYCLE_INTERVAL))
  {
    triggerSampling();
  }

  if (isSampling)
  {
    if (now - lastSampleTime >= SAMPLE_INTERVAL)
    {
      lastSampleTime = now;

      float val = distanceSensor.measureDistanceCm();
      if (val > MIN_VALID_CM && val <= BIN_HEIGHT_CM)
      {
        currentReadings.push_back(val);
      }

      static int attempts = 0;
      attempts++;

      if (attempts >= TARGET_SAMPLES)
      {
        isSampling = false;
        attempts = 0;

        Serial.printf("Collected %d valid samples.\n", currentReadings.size());
        float distance = processReadings(currentReadings);

        if (distance > 0)
        {
          int fillPercentage = map((long)distance, 0, (long)BIN_HEIGHT_CM, 100, 0);
          fillPercentage = constrain(fillPercentage, 0, 100);
          lastValidFillLevel = fillPercentage;

          Serial.printf("Fill: %d%% | Threshold: %d%%\n", fillPercentage, threshold);

          // --- ACTUATION LOGIC ---
          const int DEADZONE = 1; // 11 buffer

          if (fillPercentage > threshold)
          {
            closeBin();
          }
          else if (fillPercentage < (threshold - DEADZONE))
          {
            openBin();
          }

          float voltage = readBatteryVoltage();
          int batteryLevel = getBatteryPercentage(voltage);
          bool isTilted = false; // We know it's false because we skip loop if true

          JsonDocument doc;
          doc["deviceId"] = DEVICE_ID;
          doc["fillLevel"] = fillPercentage;
          doc["batteryPercentage"] = batteryLevel;
          doc["voltage"] = round(voltage * 100.0) / 100.0; // Round to 2 decimals
          doc["isTilted"] = isTilted;

          char output[256];
          serializeJson(doc, output);
          mqtt.publish(MQTT::Topics::getData(DEVICE_ID), output);
          Serial.print("Published: ");
          Serial.println(output);
        }
        else
        {
          Serial.println("Error: No valid readings.");
        }
      }
    }
  }

  delay(10); // Needed to trigger Modem Sleep
}