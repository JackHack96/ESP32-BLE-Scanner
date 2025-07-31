#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <time.h>

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
  "c3:60:9b:e1:45:3a",
  "cd:82:c3:49:b2:92",
  "ff:85:2e:2d:24:4b",
  "ec:86:40:c9:54:22",
  "d3:70:53:c8:14:b1",
  "d7:b3:4c:1f:9d:3d",
  "f9:6e:db:b6:20:17",
  "d1:ef:dc:25:b1:6b",
  "ec:86:40:c9:54:22",
};

//*************************************************
// Globals
//*************************************************
WiFiClient espClient;
PubSubClient mqttClient(espClient);

//*************************************************
// NTP Time Functions
//*************************************************
void initNTP() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("Waiting for NTP time synchronization...");
  
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 8 * 3600 * 2 && attempts < 50) { // Wait up to 50 seconds
    delay(1000);
    Serial.print(".");
    now = time(nullptr);
    attempts++;
  }
  
  if (now > 8 * 3600 * 2) {
    Serial.println("\nNTP synchronized successfully!");
    Serial.print("Current time: ");
    Serial.println(ctime(&now));
  } else {
    Serial.println("\nWarning: NTP synchronization failed!");
  }
}

unsigned long getTimestampMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned long)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
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
  static unsigned long lastNtpSync = 0;
  const unsigned long NTP_SYNC_INTERVAL = 3600000; // Resync every hour (3600000 ms)
  
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    initNTP(); // Re-initialize NTP after WiFi reconnection
  }

  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  // Periodic NTP resynchronization
  unsigned long currentTime = millis();
  if (currentTime - lastNtpSync > NTP_SYNC_INTERVAL) {
    Serial.println("Performing periodic NTP resync...");
    initNTP();
    lastNtpSync = currentTime;
  }

  mqttClient.loop(); // Maintain MQTT connection
  delay(1); // Minimal delay for better responsiveness
}