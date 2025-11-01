#pragma once

// Only Send LoRaWAN Logs if Enabled
#define LORAWAN_LOGGING_ENABLED 1

// Lab WiFi Credentials
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// Mobile Hotspot Credentials
// #define WIFI_SSID "YOUR_SSID"
// #define WIFI_PASSWORD "YOUR_PASSWORD"

#define ROOM_USERNAME "user"
#define ROOM_PASSWORD "password"

#define ADMIN_USERNAME_CREDENTIALS "admin"
#define ADMIN_PASSWORD_CREDENTIALS "admin"

// --- LoRaWAN Credentials ---
// End Device ID: lab3-part2-40250986
// Frequency plan: United States 902-928 MHz, FSB 2 (used by TTN)
// LoRaWAN version: LoRaWAN Specification 1.0.3
// Regional Parameters version: RP001 Regional Parameters 1.0.3 revision A

// AppEUI, DevEUI in lsb (least significant bit) format. AppKey in msb (most significant bit) format.
#define APP_EUI {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define DEV_EUI {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define APP_KEY {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
