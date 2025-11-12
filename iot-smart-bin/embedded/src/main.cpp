#include <Arduino.h>
#include <WiFi.h>
#if defined(PRODUCTION_BUILD)
#include <WebSocketsClient.h> // For WSS
#else
#include <WiFiClient.h> // For standard MQTT
#endif
#include <MQTTPubSubClient.h>

#ifdef CI_BUILD
#define WIFI_SSID "DUMMY_SSID"
#define WIFI_PASSWORD "DUMMY_PASSWORD"
#else
#include "credentials.h"
#endif

// --- WiFi Credentials ---
const char *SSID = WIFI_SSID;
const char *PASSWORD = WIFI_PASSWORD;

// --- MQTT Client Setup ---
#if defined(PRODUCTION_BUILD)
WebSocketsClient client; // For WSS (Secure WebSocket)
const char *BROKER_URL = MQTT_PROD_BROKER_URL;
const uint16_t BROKER_PORT = MQTT_PROD_BROKER_PORT;
const char *MQTT_USER = MQTT_PROD_USERNAME;
const char *MQTT_PASS = MQTT_PROD_PASSWORD;
const char *CLIENT_ID = "esp32-prod-client";
#else
WiFiClient client; // For standard unsecured MQTT
const char *BROKER_URL = MQTT_DEV_BROKER_URL;
const uint16_t BROKER_PORT = MQTT_DEV_BROKER_PORT;
const char *MQTT_USER = MQTT_DEV_USERNAME;
const char *MQTT_PASS = MQTT_DEV_PASSWORD;
const char *CLIENT_ID = "esp32-dev-client";
#endif

MQTTPubSubClient mqtt;
unsigned long lastMsgTime = 0;
const long msgInterval = 10000; // Publish every 10 seconds

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
    if (millis() - startTime > 15000)
    { // 15 second timeout
      Serial.println("\nFailed to connect to WiFi. Restarting...");
      delay(1000);
      ESP.restart();
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void mqttCallback(const String &payload, size_t size)
{
  Serial.println("--- NEW MESSAGE [test/topic] ---");
  Serial.print("Payload: ");
  Serial.println(payload);
  Serial.println("--------------------------------");
}

void connectToMqtt()
{
  Serial.print("Connecting to MQTT broker... ");

  while (!mqtt.connect(CLIENT_ID, MQTT_USER, MQTT_PASS))
  {
    Serial.print(".");
    delay(5000);
  }

  Serial.println(" MQTT Connected!");

  // Subscribe to the test topic
  mqtt.subscribe("test/topic", mqttCallback);
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }
  Serial.println("Starting IoT Smart Bin MQTT Client...");
  connectToWifi();

// --- Configure the correct client ---
#if defined(PRODUCTION_BUILD)
  Serial.println("Mode: PRODUCTION (WSS)");
  Serial.printf("Connecting to %s:%d\n", BROKER_URL, BROKER_PORT);
  client.beginSSL(BROKER_URL, BROKER_PORT);
  client.setReconnectInterval(5000);
#else
  Serial.println("Mode: DEVELOPMENT (unsecured MQTT)");
  Serial.printf("Connecting to %s:%d\n", BROKER_URL, BROKER_PORT);
  if (!client.connect(BROKER_URL, BROKER_PORT))
  {
    Serial.println("TCP connection failed! Restarting...");
    delay(1000);
    ESP.restart();
  }
  Serial.println("TCP connection successful.");
#endif

  mqtt.begin(client);

  mqtt.subscribe([](const String &topic, const String &payload, size_t size)
                 { Serial.printf("[Global] Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str()); });

  connectToMqtt();
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
  if (now - lastMsgTime > msgInterval)
  {
    lastMsgTime = now;

    String testMessage = "Hello from ESP32! Uptime: " + String(now / 1000) + "s";
    Serial.printf("Publishing message to 'test/topic': %s\n", testMessage.c_str());

    if (!mqtt.publish("test/topic", testMessage))
    {
      Serial.println("Publish failed!");
    }
  }
}