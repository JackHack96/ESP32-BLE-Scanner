# ESP32 BLE Scanner for Indoor Localization

An optimized ESP32-based BLE scanner designed for high-performance indoor localization systems. This scanner continuously monitors BLE devices, measures RSSI values, and publishes timestamped data via MQTT for real-time positioning applications.

## Features

- **High-Performance Scanning**: Optimized for 100ms continuous BLE scanning intervals for maximum accuracy
- **MQTT Integration**: Real-time data publishing to MQTT broker with format `MAC_ADDRESS,RSSI,TIMESTAMP_MS`
- **NTP Synchronization**: Precise timestamping with automatic timezone handling (Italy/CEST support)
- **WiFi Auto-Reconnection**: Robust network connectivity with automatic recovery
- **Target Device Filtering**: Pre-configured list of BLE devices for focused scanning
- **RSSI Threshold**: Configurable minimum signal strength filtering

## Technical Specifications

### BLE Scanning Configuration
- **Scan Interval**: 100ms (160 × 0.625ms units)
- **Scan Window**: 100ms (160 × 0.625ms units) - continuous scanning
- **Active Scanning**: Enabled for maximum device discovery
- **RSSI Threshold**: -80 dBm (configurable)

### Network Configuration
- **WiFi**: WPA2 Personal authentication
- **MQTT**: Publish to configurable broker and topic
- **NTP**: Automatic time synchronization with timezone support

## Hardware Requirements

- ESP32 development board or module
- WiFi network access
- MQTT broker (local or cloud-based)

## Software Dependencies

- **Arduino IDE** (1.8 or above)
- **ESP32 Board Package** for Arduino IDE
- **Required Libraries**:
  - `WiFi.h` (built-in)
  - `PubSubClient.h` (for MQTT)
  - `BLEDevice.h` (built-in ESP32 BLE)
  - `NTPClient.h` (for time synchronization)
  - `WiFiUdp.h` (for NTP)

## Installation and Configuration

### 1. Install Dependencies
In Arduino IDE, install the following libraries via Library Manager:
- `PubSubClient` by Nick O'Leary
- `NTPClient` by Fabrice Weinberg

### 2. Configure Network Settings
Edit these defines in `ESP32_BLE_Scanner.ino`:

```cpp
#define WIFI_SSID       "your-wifi-ssid"
#define WIFI_PASSWORD   "your-wifi-password"
#define MQTT_BROKER     "192.168.1.100"  // Your MQTT broker IP
#define MQTT_PORT       1883
#define MQTT_TOPIC      "localization/rssi"
```

### 3. Configure Target Devices
Update the BLE device list with your target devices:

```cpp
String addrList[] = {
  "c3:60:9b:e1:45:3a", // Device 1
  "ff:85:2e:2d:24:4b", // Device 2
  "ec:86:40:c9:54:22", // Device 3
  // Add more devices as needed
};
```

### 4. Timezone Configuration
For Italy (CEST), the default settings are:

```cpp
#define GMT_OFFSET_SEC  3600        // GMT+1
#define DAYLIGHT_OFFSET_SEC 3600    // +1 hour for DST
```

### 5. Upload and Run
1. Connect your ESP32 to your computer
2. Select the correct board and port in Arduino IDE
3. Compile and upload the sketch
4. Monitor serial output to verify operation

## MQTT Data Format

Published messages follow this format:
```
MAC_ADDRESS,RSSI,TIMESTAMP_MS
```

Example:
```
c3:60:9b:e1:45:3a,-65,1753965216123
```

Where:
- `MAC_ADDRESS`: BLE device MAC address (lowercase)
- `RSSI`: Signal strength in dBm
- `TIMESTAMP_MS`: Unix timestamp in milliseconds with timezone

## System Behavior

### Startup Sequence
1. WiFi connection establishment
2. NTP time synchronization
3. MQTT broker connection
4. BLE scanner initialization
5. Continuous scanning begins

### Operation
- Scans for BLE devices every 100ms
- Filters by target device list and RSSI threshold
- Publishes qualifying detections to MQTT immediately
- Maintains network connections with auto-reconnect
- Resyncs NTP time hourly for accuracy

### Performance Optimizations
- **Continuous Scanning**: 100% duty cycle for maximum detection rate
- **Minimal Loop Delay**: 1ms delay for optimal responsiveness
- **Efficient Filtering**: Early exit on RSSI/address mismatch
- **Reliable Timestamping**: NTPClient library for consistent time accuracy

## Indoor Localization Use Cases

This scanner is optimized for:
- **Real-time Asset Tracking**: High-frequency position updates
- **Personnel Monitoring**: Workplace safety and efficiency
- **Smart Building Systems**: Occupancy detection and automation
- **Research Applications**: Indoor positioning algorithm development

## Troubleshooting

### Common Issues
- **WiFi Connection**: Verify SSID/password and network availability
- **MQTT Connection**: Check broker IP, port, and network connectivity
- **Time Synchronization**: Ensure NTP server access and timezone settings
- **BLE Detection**: Verify target device MAC addresses and RSSI threshold

### Debug Output
Monitor serial output at 115200 baud for:
- Network connection status
- NTP synchronization confirmation
- MQTT connection status
- Real-time scanning activity

## Performance Notes

- **Scan Rate**: Achieves ~10 scans per second (100ms intervals)
- **Detection Latency**: Typically <200ms from device appearance to MQTT publish
- **Network Resilience**: Automatic reconnection with <5 second recovery
- **Time Accuracy**: ±1 second precision with hourly NTP synchronization

## License

This project is open source. Please refer to the repository license for usage terms.
