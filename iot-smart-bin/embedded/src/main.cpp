#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#ifdef CI_BUILD
// When building in a CI environment, use dummy credentials
#define WIFI_SSID "DUMMY_SSID"
#define WIFI_PASSWORD "DUMMY_PASSWORD"
#else
// For local builds, include the actual credentials file
#include "credentials.h"
#endif

// --- WiFi Credentials ---
const char *SSID = WIFI_SSID;
const char *PASSWORD = WIFI_PASSWORD;

// --- Global Variables ---
const unsigned char PORT = 80;
AsyncWebServer server(PORT);

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

void setupWebServer()
{
  // --- Serve static files from SPIFFS ---
  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  //           {
  //   if (isLockedOut()) {
  //     request->redirect("/cooldown.html");
  //   } else {
  //     request->send(SPIFFS, "/index.html", "text/html");
  //   } });

  // server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
  //           { request->send(SPIFFS, "/script.js", "text/javascript"); });

  // --- Handle API Requests ---
  // server.on("/cooldown-time", HTTP_GET, [](AsyncWebServerRequest *request)
  //           {
  //   Serial.println("[INFO] Cooldown time requested: " + String(remainingLockoutTimeMs) + " ms remaining.");
  //   if (isLockedOut()) {
  //     request->send(200, "text/plain", String(remainingLockoutTimeMs));
  //   } else {
  //     // Not locked out, send 0
  //     request->send(200, "text/plain", "0");
  //   } });

  // --- Other Paths ---
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->redirect("/"); });

  server.begin();
  Serial.println(String("HTTP server started: Accessible at http://") + WiFi.localIP().toString() + ":" + String(PORT));
}

void setup()
{
  // Wait for Serial to be ready
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  connectToWifi();

  setupWebServer();
}

void loop()
{
  // put your main code here, to run repeatedly:
}