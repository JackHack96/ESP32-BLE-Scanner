#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_bt.h>

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

// RSSI Averaging and Beacon Tracking
#define MAX_BEACONS 10
#define RSSI_SAMPLES 5
#define PUBLISH_INTERVAL_MS 1000

struct BeaconData {
  String address;
  int rssiBuffer[RSSI_SAMPLES];
  int bufferIndex;
  int sampleCount;
  unsigned long lastSeen;
  unsigned long lastPublished;
  bool isActive;
};

BeaconData beacons[MAX_BEACONS];
int beaconCount = 0;

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

inline unsigned long getTimestamp() {
  return timeClient.getEpochTime();
}

//*************************************************
// RSSI Averaging Functions
//*************************************************
int findOrCreateBeacon(const String& address) {
  // Find existing beacon
  for (int i = 0; i < beaconCount; i++) {
    if (beacons[i].address == address) {
      return i;
    }
  }
  
  // Create new beacon if space available
  if (beaconCount < MAX_BEACONS) {
    int index = beaconCount++;
    beacons[index].address = address;
    beacons[index].bufferIndex = 0;
    beacons[index].sampleCount = 0;
    beacons[index].lastSeen = millis();
    beacons[index].lastPublished = 0;
    beacons[index].isActive = true;
    return index;
  }
  
  return -1; // No space available
}

void addRSSISample(int beaconIndex, int rssi) {
  BeaconData& beacon = beacons[beaconIndex];
  beacon.rssiBuffer[beacon.bufferIndex] = rssi;
  beacon.bufferIndex = (beacon.bufferIndex + 1) % RSSI_SAMPLES;
  if (beacon.sampleCount < RSSI_SAMPLES) {
    beacon.sampleCount++;
  }
  beacon.lastSeen = millis();
}

int getAverageRSSI(int beaconIndex) {
  BeaconData& beacon = beacons[beaconIndex];
  if (beacon.sampleCount == 0) return -100;
  
  long sum = 0;
  for (int i = 0; i < beacon.sampleCount; i++) {
    sum += beacon.rssiBuffer[i];
  }
  return sum / beacon.sampleCount;
}

void publishBeaconData() {
  unsigned long currentTime = millis();
  
  for (int i = 0; i < beaconCount; i++) {
    BeaconData& beacon = beacons[i];
    
    // Skip if not enough samples or published recently
    if (beacon.sampleCount < 3 || 
        (currentTime - beacon.lastPublished) < PUBLISH_INTERVAL_MS) {
      continue;
    }
    
    // Check if beacon is still active (seen within last 5 seconds)
    if ((currentTime - beacon.lastSeen) > 5000) {
      beacon.isActive = false;
      continue;
    }
    
    int avgRSSI = getAverageRSSI(i);
    if (avgRSSI >= MIN_RSSI) {
      unsigned long timestamp = getTimestamp();
      char payload[150];
      snprintf(payload, sizeof(payload), "%s,%d,%lu", 
               beacon.address.c_str(), avgRSSI, timestamp);
      
      if (mqttClient.publish(MQTT_TOPIC, payload)) {
        beacon.lastPublished = currentTime;
      }
    }
  }
}

//*************************************************
// BLE Device Callback
//*************************************************
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    String addr = device.getAddress().toString().c_str();
    addr.toLowerCase(); // Normalize to lowercase
    int rssi = device.getRSSI();

    // Check if this is a target device
    bool isTargetDevice = false;
    for (int j = 0; j < sizeof(addrList) / sizeof(addrList[0]); j++) {
      if (addrList[j].equalsIgnoreCase(addr)) {
        isTargetDevice = true;
        break;
      }
    }
    
    if (isTargetDevice && rssi >= MIN_RSSI) {
      int beaconIndex = findOrCreateBeacon(addr);
      if (beaconIndex >= 0) {
        addRSSISample(beaconIndex, rssi);
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
  
  // Initialize beacon tracking
  beaconCount = 0;
  
  // Configure WiFi/BLE coexistence
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Minimize WiFi power saving for better coexistence
  
  connectToWiFi();

  // Initialize NTP after WiFi connection
  initNTP();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  connectToMQTT();

  // Configure BLE with coexistence settings
  BLEDevice::init("");
  
  // Set BLE power to reduce interference
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P3); // +3dBm
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P3);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P3);
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  
  // Optimize scan parameters for coexistence
  pBLEScan->setInterval(200);  // 200 * 0.625ms = 125ms interval
  pBLEScan->setWindow(100);    // 100 * 0.625ms = 62.5ms scan window (50% duty cycle)
  pBLEScan->setActiveScan(false); // Passive scanning reduces interference
  
  Serial.println("Starting BLE scan with WiFi coexistence...");
  pBLEScan->start(0, nullptr, false); // Continuous scanning
}

//*************************************************
// Loop
//*************************************************
void loop() {
  static unsigned long lastWiFiCheck = 0;
  static unsigned long lastNTPUpdate = 0;
  static unsigned long lastBeaconPublish = 0;
  
  unsigned long currentTime = millis();
  
  // Check WiFi connection every 5 seconds
  if (currentTime - lastWiFiCheck > 5000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      connectToWiFi();
      initNTP(); // Re-initialize NTP after WiFi reconnection
    }
    lastWiFiCheck = currentTime;
  }

  // Check MQTT connection
  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  // Update NTP every 30 seconds (less frequent to reduce interference)
  if (currentTime - lastNTPUpdate > 30000) {
    timeClient.update();
    lastNTPUpdate = currentTime;
  }
  
  // Publish beacon data every 500ms
  if (currentTime - lastBeaconPublish > 500) {
    publishBeaconData();
    lastBeaconPublish = currentTime;
  }

  mqttClient.loop(); // Maintain MQTT connection
  
  // Longer delay to give more time for BLE scanning
  delay(10);
}