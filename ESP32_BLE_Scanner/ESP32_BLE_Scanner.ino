#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

//*************************************************
// Configuration
//*************************************************
#define WIFI_SSID       "matteo-XPS-15-9570"
#define WIFI_PASSWORD   "password"

#define MQTT_BROKER     "10.42.0.1"  // Replace with your MQTT broker IP
#define MQTT_PORT       1883
#define MQTT_TOPIC      "localization/rssi"

#define MIN_RSSI        -80

// NTP Configuration
#define NTP_SERVER      "pool.ntp.org"
#define GMT_OFFSET_SEC  3600   // Italy: GMT+1 (Central European Time)
#define DAYLIGHT_OFFSET_SEC 3600  // Italy: +1 hour for daylight saving time (CEST)

// BLE MAC addresses of interest
String addrList[] = {
  "c3:60:9b:e1:45:3a", // Tavolo
  "ff:85:2e:2d:24:4b", // Cucina
  "ec:86:40:c9:54:22", // Porta
  "d3:70:53:c8:14:b1", // Divano
  "f9:6e:db:b6:20:17", // Lavagna
};

//*************************************************
// Globals
//*************************************************
WiFiClient espClient;
PubSubClient mqttClient(espClient);
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//*************************************************
// NTP Time Functions
//*************************************************
void initNTP() {
  timeClient.begin();
  timeClient.setPoolServerName(NTP_SERVER);
  timeClient.setTimeOffset(GMT_OFFSET_SEC);
  timeClient.setUpdateInterval(60000); // Update every minute

  Serial.println("Synchronizing time with NTP server...");
  
  // Wait for NTP to sync
  while (!timeClient.update()) {
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println("\nNTP synchronization complete.");
  Serial.print("Current time: ");
  Serial.println(timeClient.getFormattedTime());
}

inline unsigned long getTimestampMs() {
  return timeClient.getEpochTime();
}

//*************************************************
// BLE Device Callback
//*************************************************
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    String addr = device.getAddress().toString().c_str();
    int rssi = device.getRSSI();

    for (int j = 0; j < sizeof(addrList) / sizeof(addrList[0]); j++) {
      if (rssi >= MIN_RSSI && addrList[j].equalsIgnoreCase(addr)) {
        unsigned long timestamp = getTimestampMs();
        char payload[150];
        snprintf(payload, sizeof(payload), "%s,%d,%lu", addr.c_str(), rssi, timestamp);
        mqttClient.publish(MQTT_TOPIC, payload);
        break;
      }
    }
  }
};

//*************************************************
// Wi-Fi Connection (WPA2 Personal)
//*************************************************
void connectToWiFi() {
  WiFi.disconnect(true);  // Reset Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
}

//*************************************************
// MQTT Connection
//*************************************************
void connectToMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32_Localizer")) {
      Serial.println("connected.");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      delay(1000);
    }
  }
}

//*************************************************
// Setup
//*************************************************
void setup() {
  Serial.begin(115200);
  connectToWiFi();

  // Initialize NTP after WiFi connection
  initNTP();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  connectToMQTT();

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true); // Want duplicates
  pBLEScan->setInterval(160);  // 160 * 0.625ms = 100ms interval - faster scanning
  pBLEScan->setWindow(160);    // 160 * 0.625ms = 100ms scan window - continuous
  pBLEScan->setActiveScan(true);
  pBLEScan->start(0, nullptr, false); // Continuous scanning
}

//*************************************************
// Loop
//*************************************************
void loop() {
 if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    initNTP(); // Re-initialize NTP after WiFi reconnection
  }

  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }

  mqttClient.loop(); // Maintain MQTT connection
  delay(1); // Minimal delay for better responsiveness
}